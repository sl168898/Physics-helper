#pragma once
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include "CorpseExplosion.h"
#include "DamageObservation.h"
#include <atomic>
#include <cctype>
#include <mutex>
#include <string>
#include <string_view>

namespace corpse
{
    class Runtime {
        inline static Runtime* self{};
        inline static thread_local Frame* active{};
        inline static thread_local bool applying{};
        inline static REL::Relocation<void (*)(RE::PlayerCharacter*, float)> originalUpdate;
        RE::SpellItem* trait{};
        std::array<RE::SpellItem*, 4> spells{};
        std::array<RE::EffectSetting*, 4> effects{};
        std::array<RE::BGSExplosion*, 4> visuals{};
        std::atomic_bool session{}, taskQueued{};
        std::atomic<std::uint64_t> epoch{};
        bool ready{}, wasSelected{};
        float maintenance{};
        std::mutex mutex;
        Ledger ledger;
        std::atomic_uint diagnosticCount{};
        static constexpr std::array<RE::ActorValue, 4> resistances{
            RE::ActorValue::kResistFire, RE::ActorValue::kResistFrost,
            RE::ActorValue::kResistShock, RE::ActorValue::kPoisonResist};
        static constexpr std::array<const char*, 4> names{"fire", "frost", "shock", "poison"};

        static float health(RE::Actor* actor) {
            return actor ? actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) : 0.f;
        }
        static bool alive(RE::Actor* actor) {
            return actor && actor->AsActorState()->GetLifeState() == RE::ACTOR_LIFE_STATE::kAlive;
        }
        static bool killQueued(RE::Actor* actor) {
            const auto process = actor ? actor->GetMiddleHighProcess() : nullptr;
            return process && process->killQueued;
        }
        bool selected() const {
            const auto p = RE::PlayerCharacter::GetSingleton();
            return ready && session.load() && p && trait && p->HasSpell(trait);
        }
        bool ownEffect(RE::EffectSetting* effect) const {
            return effect && std::find(effects.begin(), effects.end(), effect) != effects.end();
        }
        static Type typeOf(RE::EffectSetting* base) {
            // Inspect the effect, not the item's IsPoison flag: elemental oils
            // are alchemy poisons too. Weakness effects never enter this path
            // unless the native operation actually damages Health.
            if (base->HasKeywordString("MagicDamageFire")) return Type::fire;
            if (base->HasKeywordString("MagicDamageFrost")) return Type::frost;
            if (base->HasKeywordString("MagicDamageShock")) return Type::shock;
            for (unsigned i = 0; i < 3; ++i)
                if (base->data.resistVariable == resistances[i]) return static_cast<Type>(i);
            return Type::poison;
        }
        Origin effectOrigin(RE::ActiveEffect* effect, RE::Actor* actor) const {
            Origin origin;
            if (!effect || !effect->effect || !effect->effect->baseEffect || !effect->spell ||
                !harvest::matchesMagicTarget(effect->target, actor)) return origin;
            const auto base = effect->effect->baseEffect;
            origin.explosion = applying || ownEffect(base);
            origin.player = effect->GetCasterActor().get() == RE::PlayerCharacter::GetSingleton();
            if (!origin.player || origin.explosion || (!base->IsHostile() && !base->IsDetrimental()) ||
                effect->flags.any(RE::ActiveEffect::Flag::kDispelled) ||
                effect->conditionStatus == RE::ActiveEffect::ConditionStatus::kFalse) return origin;
            if (const auto oil = effect->spell->As<RE::AlchemyItem>(); oil && oil->IsPoison()) {
                origin.poison = oil->GetFormID(); origin.type = typeOf(base);
            }
            return origin;
        }
        void configureVisuals(RE::TESDataHandler* data) {
            const auto fallback = data->LookupForm<RE::BGSExplosion>(0xF5076, "Skyrim.esm");
            for (unsigned i = 0; i < 4; ++i) {
                RE::BGSExplosion* best = fallback; int bestScore = 0;
                for (auto candidate : data->GetFormArray<RE::BGSExplosion>()) {
                    if (!candidate || std::find(visuals.begin(), visuals.end(), candidate) != visuals.end()) continue;
                    const auto path = candidate->GetModel();
                    if (!path || !*path) continue;
                    std::string model = path;
                    std::transform(model.begin(), model.end(), model.begin(), [](unsigned char c) { return std::tolower(c); });
                    int score = model.find(names[i]) != std::string::npos ? 10 : 0;
                    if (i == 2 && model.find("lightning") != std::string::npos) score = 10;
                    if (i == 3 && model.find("spider") != std::string::npos) score = 10;
                    if (!score) continue;
                    if (model.find("explosion") != std::string::npos) score += 5;
                    if (model.find("fireball") != std::string::npos) score += 5;
                    if (score > bestScore) { bestScore = score; best = candidate; }
                }
                auto visual = visuals[i];
                if (best) {
                    visual->SetModel(best->GetModel());
                    visual->data.light = best->data.light;
                    visual->data.sound1 = best->data.sound1;
                    visual->data.sound2 = best->data.sound2;
                }
                // Only copy presentation from loaded forms. No native blast
                // damage, force, enchantment, hazards, projectiles or chains.
                visual->formEnchanting = nullptr;
                visual->data.damage = visual->data.force = 0.f;
                visual->data.radius = radius;
                visual->data.imageSpaceRadius = 0.f;
                visual->data.impactPlacedObject = nullptr;
                visual->data.spawnProjectile = nullptr;
                visual->data.impactDataSet = nullptr;
                visual->data.flags = RE::BGSExplosionData::Flag::kIgnoreImageSpaceSwap;
                SKSE::log::info("[CorpseExplosion] {} visual: {}", names[i], visual->GetModel());
            }
        }
        static bool enemy(RE::Actor* actor, RE::Actor* player) {
            if (!actor || actor == player || !alive(actor) || actor->IsDisabled() ||
                actor->IsPlayerTeammate() || !actor->Is3DLoaded() || health(actor) <= 0) return false;
            if (const auto commander = actor->GetCommandingActor(); commander &&
                (commander.get() == player || commander->IsPlayerTeammate())) return false;
            return actor->IsHostileToActor(player);
        }
        void explode(RE::Actor* body, const Damage& damage) {
            const auto p = RE::PlayerCharacter::GetSingleton();
            const auto world = RE::TES::GetSingleton();
            if (!p || !world || !body->Is3DLoaded() || body->IsDisabled()) return;
            const auto dominant = std::distance(damage.begin(), std::max_element(damage.begin(), damage.end()));
            body->PlaceObjectAtMe(visuals[dominant], false);
            // Collect references first; native casts must not run under a cell
            // reference-list spinlock held by ForEachReferenceInRange.
            std::vector<RE::ActorHandle> actors;
            world->ForEachReferenceInRange(body, radius, [&](RE::TESObjectREFR* ref) {
                if (auto a = ref ? ref->As<RE::Actor>() : nullptr; a && a != body)
                    actors.push_back(a->GetHandle());
                return RE::BSContainer::ForEachResult::kContinue;
            });
            auto caster = p->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
            if (!caster) return;
            struct Applying { bool old = applying; Applying() { applying = true; } ~Applying() { applying = old; } } scope;
            unsigned hit{};
            for (auto handle : actors) {
                auto a = handle.get();
                if (!enemy(a.get(), p)) continue;
                bool visible = false;
                if (!a->HasLineOfSight(body, visible)) continue;
                ++hit;
                for (unsigned i = 0; i < 4 && alive(a.get()) && health(a.get()) > 0 && !killQueued(a.get()); ++i) {
                    const auto resist = a->AsActorValueOwner()->GetActorValue(resistances[i]);
                    const double amount = damage[i] * resistanceMultiplier(resist);
                    if (!(amount > 0) || !std::isfinite(amount) || amount > std::numeric_limits<float>::max()) continue;
                    // Dedicated effects bypass engine resistance/absorption:
                    // matching alchemical resistance is applied above exactly
                    // once. Generic MagicResist and PoisonResist must not
                    // accidentally resist elemental oils. Native health,
                    // difficulty, essential status and kill attribution remain.
                    caster->CastSpellImmediate(spells[i], true, a.get(), 1.f, false, static_cast<float>(amount), p);
                    SKSE::log::info("[CorpseExplosion] victim={:08X} target={:08X} type={} base={} resistance={} delivered={}",
                        body->GetFormID(), a->GetFormID(), names[i], damage[i], resist, amount);
                }
            }
            SKSE::log::info("[CorpseExplosion] burst victim={:08X} enemies={} radius={}; fire={} frost={} shock={} poison={}",
                body->GetFormID(), hit, radius, damage[0], damage[1], damage[2], damage[3]);
        }
        void pump(std::uint64_t generation) {
            if (generation != epoch.load()) return;
            taskQueued.store(false);
            if (!selected()) return;
            std::vector<ID> pending;
            { std::lock_guard lock(mutex); for (const auto& [id, t] : ledger.targets)
                if (t.state == State::pending) pending.push_back(id); }
            for (auto id : pending) {
                const RE::NiPointer<RE::Actor> body(RE::TESForm::LookupByID<RE::Actor>(id));
                if (!body) { forget(id); continue; }
                if (!body->IsDead()) {
                    // A new killQueued flag proves attribution, but wait for
                    // actual death before spawning the visible corpse burst.
                    if (!killQueued(body.get()) && health(body.get()) > 0) forget(id);
                    continue;
                }
                std::optional<Damage> damage;
                { std::lock_guard lock(mutex); damage = ledger.claim(id); }
                if (damage && generation == epoch.load() && selected()) explode(body.get(), *damage);
            }
        }
        void queue() {
            if (!selected() || taskQueued.exchange(true)) return;
            const auto generation = epoch.load();
            SKSE::GetTaskInterface()->AddTask([generation] { if (self) self->pump(generation); });
        }
        void update(float delta) {
            const bool on = selected();
            if (!on) {
                if (wasSelected) { std::lock_guard lock(mutex); ledger.clear(); }
                wasSelected = false; return;
            }
            wasSelected = true;
            bool pending = false;
            std::vector<ID> check;
            maintenance += std::isfinite(delta) ? std::max(0.f, delta) : 0.f;
            {
                std::lock_guard lock(mutex);
                for (const auto& [id, t] : ledger.targets) {
                    pending |= t.state == State::pending;
                    if (maintenance >= 1.f && t.state != State::pending) check.push_back(id);
                }
            }
            if (pending) queue();
            if (maintenance < 1.f) return;
            maintenance = 0.f;
            for (auto id : check) {
                auto a = RE::TESForm::LookupByID<RE::Actor>(id);
                if (!a) { forget(id); continue; }
                // A fully recovered target outside combat starts a new fight.
                // Partly injured stationary test targets retain their damage.
                if (alive(a) && !a->IsInCombat() &&
                    a->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth) >= -0.01f)
                    forget(id);
            }
        }
        static void Update(RE::PlayerCharacter* p, float delta) {
            originalUpdate(p, delta);
            if (self) self->update(delta);
        }
        template<class T> struct HealthHook {
            inline static REL::Relocation<void (*)(RE::Actor*, RE::Actor*, float)> original;
            static void Damage(RE::Actor* actor, RE::Actor* attacker, float damage) {
                Origin origin{attacker == RE::PlayerCharacter::GetSingleton(), applying};
                if (!attacker || origin.player)
                    for (auto f = active; f; f = f->parent) if (f->actor == actor->GetFormID()) { origin = f->origin; break; }
                Scope observed(actor, origin, true);
                original(actor, attacker, damage);
                observed.finish();
            }
            static void install() {
                REL::Relocation<std::uintptr_t> table{T::VTABLE[0]};
                original = table.write_vfunc(0x104, Damage);
            }
        };
    public:
        class Scope {
            Runtime* runtime{};
            RE::NiPointer<RE::Actor> actor;
            Frame frame;
            std::uint64_t generation{};
            bool queuedBefore{}, essential{}, finished{};
        public:
            Scope(RE::Actor* a, Origin origin, bool healthChange) {
                essential = a && a->GetActorRuntimeData().boolFlags.any(RE::Actor::BOOL_FLAGS::kEssential);
                if (!healthChange || !self || !self->selected() || !a ||
                    !harvest::canObserveHealthBefore(a->AsActorState()->GetLifeState(), essential) ||
                    a == RE::PlayerCharacter::GetSingleton() || health(a) <= 0) return;
                runtime = self; actor = RE::NiPointer<RE::Actor>(a); generation = runtime->epoch.load();
                frame = {a->GetFormID(), health(a), 0, origin, active, std::nullopt};
                active = &frame;
                queuedBefore = killQueued(a);
                if (!queuedBefore) { std::lock_guard lock(runtime->mutex); runtime->ledger.resurrected(frame.actor); }
            }
            Scope(RE::ActiveEffect* effect, RE::Actor* a, bool healthChange) :
                Scope(a, self && healthChange && self->selected() ? self->effectOrigin(effect, a) : Origin{}, healthChange) {}
            Scope(const Scope&) = delete;
            ~Scope() { finish(); }
            void finish() {
                if (!runtime || finished) return;
                finished = true; active = frame.parent;
                const auto after = health(actor.get());
                const auto state = actor->AsActorState()->GetLifeState();
                const auto lethal = harvest::isLethalHealthChange({true, true, static_cast<float>(frame.before), after,
                    queuedBefore, killQueued(actor.get()),
                    state == RE::ACTOR_LIFE_STATE::kDying || state == RE::ACTOR_LIFE_STATE::kDead, essential});
                const auto result = frame.finish(after, lethal);
                if (generation != runtime->epoch.load() || !runtime->selected()) return;
                bool offered = false;
                {
                    std::lock_guard lock(runtime->mutex);
                    runtime->ledger.record(frame.actor, frame.origin, result.damage);
                    if (result.death) offered = runtime->ledger.killed(frame.actor, *result.death);
                }
                if (frame.origin.player && !frame.origin.explosion && result.damage > 0 && runtime->diagnosticCount.fetch_add(1) < 100)
                    SKSE::log::info("[CorpseExplosion] damage victim={:08X} source={:08X} type={} actual={}",
                        frame.actor, frame.origin.poison, names[static_cast<unsigned>(frame.origin.type)], result.damage);
                if (offered) {
                    SKSE::log::info("[CorpseExplosion] confirmed poison/oil killing blow victim={:08X} source={:08X}",
                        frame.actor, result.death->poison);
                    runtime->queue();
                }
            }
        };
        void init() {
            self = this;
            const auto data = RE::TESDataHandler::GetSingleton();
            constexpr auto plugin = "Biggie Traits - Combined.esp";
            trait = data->LookupForm<RE::SpellItem>(0xF80, plugin);
            ready = trait != nullptr;
            for (unsigned i = 0; i < 4; ++i) {
                effects[i] = data->LookupForm<RE::EffectSetting>(0xF82 + 2 * i, plugin);
                spells[i] = data->LookupForm<RE::SpellItem>(0xF83 + 2 * i, plugin);
                visuals[i] = data->LookupForm<RE::BGSExplosion>(0xF8A + i, plugin);
                ready &= effects[i] && spells[i] && visuals[i];
            }
            if (!ready) { SKSE::log::error("[CorpseExplosion] records missing; trait disabled (requires Combined 2.13.0)"); return; }
            constexpr std::array<std::string_view, 4> typeKeywords{
                "MagicDamageFire", "MagicDamageFrost", "MagicDamageShock", "MagicDamagePoison"};
            for (auto keyword : data->GetFormArray<RE::BGSKeyword>()) {
                if (!keyword) continue;
                const auto editor = keyword->GetFormEditorID();
                if (!editor) continue;
                for (unsigned i = 0; i < 4; ++i)
                    if (typeKeywords[i] == editor) effects[i]->AddKeyword(keyword);
            }
            configureVisuals(data);
            HealthHook<RE::Actor>::install(); HealthHook<RE::Character>::install();
            REL::Relocation<std::uintptr_t> table{RE::VTABLE_PlayerCharacter[0]};
            originalUpdate = table.write_vfunc(0xAD, Update);
            SKSE::log::info("[CorpseExplosion] ready: 25 percent, radius 420, elemental/poison resistance, strict oil kill, no chains");
        }
        void setSession(bool value) { session.store(value); }
        void reset() {
            ++epoch; taskQueued.store(false); diagnosticCount.store(0); maintenance = 0;
            std::lock_guard lock(mutex); ledger.clear(); wasSelected = false;
        }
        void forget(ID id) { std::lock_guard lock(mutex); ledger.forget(id); }
        void save(SKSE::SerializationInterface* api) {
            std::lock_guard lock(mutex); const auto bytes = encode(ledger);
            if (!api->WriteRecord(recordID, 1, bytes.data(), static_cast<std::uint32_t>(bytes.size())))
                SKSE::log::error("[CorpseExplosion] save failed");
        }
        void load(SKSE::SerializationInterface* api, std::uint32_t version, std::uint32_t size) {
            if (version != 1 || size > 32 * 1024 * 1024) return;
            std::vector<std::uint8_t> bytes(size);
            if (api->ReadRecordData(bytes.data(), size) != size) return;
            auto restored = decode(bytes, [api](ID old) { RE::FormID id{}; return api->ResolveFormID(old, id) ? id : 0; });
            if (restored) {
                std::lock_guard lock(mutex); ledger = std::move(*restored);
                SKSE::log::info("[CorpseExplosion] restored {} tracked targets", ledger.targets.size());
            } else SKSE::log::error("[CorpseExplosion] invalid save record discarded");
        }
    };
}
