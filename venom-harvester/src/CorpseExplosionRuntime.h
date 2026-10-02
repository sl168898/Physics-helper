#pragma once
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include "CorpseExplosion.h"
#include "DamageObservation.h"
#include "BlastTargets.h"
#include "BlastDelivery.h"
#include <atomic>
#include <cctype>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>

namespace corpse
{
    class Runtime {
        inline static Runtime* self{};
        inline static thread_local Frame* active{};
        inline static thread_local bool applying{};
        struct AreaCast {
            BlastDelivery delivery;
            RE::NiPointer<RE::Actor> body, player;
        };
        inline static thread_local AreaCast* areaCast{};
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
            const auto burst = std::find(effects.begin(), effects.end(), base);
            origin.explosion = applying || burst != effects.end();
            origin.player = effect->GetCasterActor().get() == RE::PlayerCharacter::GetSingleton();
            if (!origin.player || (!base->IsHostile() && !base->IsDetrimental()) ||
                effect->flags.any(RE::ActiveEffect::Flag::kDispelled) ||
                effect->conditionStatus == RE::ActiveEffect::ConditionStatus::kFalse) return origin;
            if (burst != effects.end()) {
                const auto index = static_cast<unsigned>(burst - effects.begin());
                if (effect->spell == spells[index]) {
                    origin.poison = effect->spell->GetFormID();
                    origin.type = static_cast<Type>(index);
                }
                return origin;
            }
            // Other effects fired during a burst are not automatically chains.
            if (origin.explosion) return origin;
            if (const auto oil = effect->spell->As<RE::AlchemyItem>(); oil && oil->IsPoison()) {
                origin.poison = oil->GetFormID(); origin.type = typeOf(base);
            }
            return origin;
        }
        void configureVisuals(RE::TESDataHandler* data) {
            const auto fallback = data->LookupForm<RE::BGSExplosion>(0xF5076, "Skyrim.esm");
            // Poison Nova links this explosion in the supplied Magic Redone
            // plugin. Prefer its winning loaded record over filename scores.
            const auto poisonNova = data->LookupForm<RE::BGSExplosion>(0x5C32, "Requiem - Magic Redone.esp");
            for (unsigned i = 0; i < 4; ++i) {
                RE::BGSExplosion* best = fallback; int bestScore = 0;
                const bool preferred = i == 3 && poisonNova && poisonNova->GetModel() && *poisonNova->GetModel();
                if (preferred) {
                    best = poisonNova;
                    SKSE::log::info("[CorpseExplosion] poison visual source: Poison Nova, Requiem - Magic Redone.esp local EXPL 005C32");
                } else for (auto candidate : data->GetFormArray<RE::BGSExplosion>()) {
                    if (!candidate || std::find(visuals.begin(), visuals.end(), candidate) != visuals.end()) continue;
                    const auto path = candidate->GetModel();
                    if (!path || !*path) continue;
                    std::string model = path;
                    std::transform(model.begin(), model.end(), model.begin(), [](unsigned char c) { return std::tolower(c); });
                    int score = model.find(names[i]) != std::string::npos ? 10 : 0;
                    if (i == 2 && model.find("lightning") != std::string::npos) score = 10;
                    // A spider WEB strip is not a poison burst. Only accept
                    // explicit poison presentations, otherwise use the known
                    // vanilla shout shockwave already shipped in the ESP.
                    if (i == 3 && (model.find("web") != std::string::npos ||
                        model.find("strip") != std::string::npos)) continue;
                    if (!score) continue;
                    if (model.find("explosion") != std::string::npos) score += 5;
                    if (model.find("fireball") != std::string::npos) score += 5;
                    if (score > bestScore) { bestScore = score; best = candidate; }
                }
                if (i == 3 && !preferred)
                    SKSE::log::warn("[CorpseExplosion] Poison Nova explosion unavailable; using loaded poison visual fallback");
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
                visual->data.flags.set(RE::BGSExplosionData::Flag::kNoControllerVibration);
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
        template<const auto& VTables> struct AreaTargetHook {
            static_assert(VTables.size() >= 10, "Actor hooks require the explicit RE::VTABLE_Actor/Character/PlayerCharacter table");
            inline static REL::Relocation<bool (*)(RE::MagicTarget*, RE::MagicTarget::AddTargetData&)> original;
            static bool Add(RE::MagicTarget* target, RE::MagicTarget::AddTargetData& data) {
                auto runtime = self;
                if (!runtime || !data.magicItem) return original(target, data);
                const auto found = std::find(runtime->spells.begin(), runtime->spells.end(), data.magicItem);
                if (found == runtime->spells.end()) return original(target, data);
                const auto index = static_cast<unsigned>(found - runtime->spells.begin());
                auto cast = areaCast;
                // Never let a console cast, deferred stray request, or native
                // splash hit unfiltered actors. Ordinary magic is forwarded.
                if (!cast || static_cast<unsigned>(cast->delivery.type) != index ||
                    !data.effect || data.effect->baseEffect != runtime->effects[index]) {
                    SKSE::log::warn("[CorpseExplosion] area-rejected type={} reason=no-matching-cast", names[index]);
                    return false;
                }
                const auto ref = target ? target->GetTargetStatsObject() : nullptr;
                const RE::NiPointer<RE::Actor> actor(ref ? ref->As<RE::Actor>() : nullptr);
                if (!eligibleBlastActor(cast->body.get(), actor.get(), radius,
                    [cast](RE::Actor* a) { return enemy(a, cast->player.get()) && !killQueued(a); })) return false;
                const auto resistance = actor->AsActorValueOwner()->GetActorValue(resistances[index]);
                const auto quote = cast->delivery.claim(actor->GetFormID(), resistance);
                if (quote.result != BlastDelivery::Result::ready) {
                    SKSE::log::info("[CorpseExplosion] area-filter victim={:08X} target={:08X} type={} result={} resistance={}",
                        cast->delivery.victim, actor->GetFormID(), names[index], static_cast<unsigned>(quote.result), resistance);
                    return false;
                }
                const float incoming = data.magnitude, before = health(actor.get());
                // The corpse determines origin; the player determines damage
                // ownership. This is the final per-target magnitude entering
                // native effect creation, with matching resistance applied once.
                data.caster = cast->player.get();
                data.magnitude = quote.magnitude;
                // Observe synchronous damage even if the engine routes it
                // straight through HandleHealthDamage. Later effect updates
                // independently recover the same qualified spell/type above.
                Scope observed(actor.get(), Origin{true, true, data.magicItem->GetFormID(),
                    static_cast<Type>(index)}, true);
                const bool accepted = original(target, data);
                observed.finish();
                if (accepted) ++cast->delivery.accepted;
                SKSE::log::info("[CorpseExplosion] area-apply victim={:08X} target={:08X} type={} base={} resistance={} engine-input={} requested={} accepted={} Health-now {} -> {}",
                    cast->delivery.victim, actor->GetFormID(), names[index], cast->delivery.base, resistance,
                    incoming, quote.magnitude, accepted, before, health(actor.get()));
                return accepted;
            }
            static void install(std::string_view name) {
                // TESObjectREFR owns four vtables. Actor's next secondary
                // base is MagicTarget (0xA0 on 1.6.1170), whose slot 1 is
                // AddTarget. Use the real secondary-base this pointer.
                // The pinned CommonLib Actor has no own VTABLE member:
                // Actor::VTABLE inherits TESObjectREFR's FOUR-entry array.
                // Pass the explicit actor array and bounds-check at compile time.
                SKSE::log::info("[CorpseExplosion] installing {} MagicTarget::AddTarget hook", name);
                REL::Relocation<std::uintptr_t> table{std::get<4>(VTables)};
                original = table.write_vfunc(1, Add);
                SKSE::log::info("[CorpseExplosion] installed {} MagicTarget::AddTarget hook", name);
            }
        };
        void explode(RE::Actor* body, const Damage& damage) {
            const auto p = RE::PlayerCharacter::GetSingleton();
            const auto processes = RE::ProcessLists::GetSingleton();
            if (!p || !processes || !body || !body->GetParentCell() ||
                !body->Is3DLoaded() || body->IsDisabled()) return;
            // The pinned CommonLib TES facade reads the wrong world-space
            // field on 1.6.1170; its sky-cell lookup crashed at this call site.
            // Enumerate actor process handles instead, then apply our own
            // 3D radius and interior/exterior-space checks.
            const auto actors = collectBlastActors<RE::BSContainer::ForEachResult::kContinue>(
                processes, body, radius);
            SKSE::log::info("[CorpseExplosion] target scan victim={:08X} loaded candidates={} radius={}",
                body->GetFormID(), actors.size(), radius);
            std::set<ID> allowed;
            for (auto handle : actors) {
                auto a = handle.get();
                if (!eligibleBlastActor(body, a.get(), radius,
                    [p](RE::Actor* actor) { return enemy(actor, p) && !killQueued(actor); })) {
                    if (a) SKSE::log::info("[CorpseExplosion] candidate target={:08X} name='{}' eligible=false", a->GetFormID(), a->GetName());
                    continue;
                }
                // Actor::HasLineOfSight rejected every eligible recipient in
                // the user's 2.1.6 log. Let native area LOS handle obstruction;
                // recipients do not have to visually perceive the dead caster.
                allowed.insert(a->GetFormID());
                SKSE::log::info("[CorpseExplosion] candidate target={:08X} name='{}' eligible=true visibility=native-area", a->GetFormID(), a->GetName());
            }
            // Ordinator Corpse Gas casts a Self-area spell from the dying
            // actor; its MGEF's Explosion drives presentation. Do the same
            // through the native instant caster, with player blame and our
            // own enemy/resistance gate at actual native effect application.
            auto caster = body->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
            if (!caster) { SKSE::log::warn("[CorpseExplosion] corpse has no instant caster"); return; }
            unsigned accepted{};
            for (unsigned i = 0; i < 4; ++i) {
                if (!(damage[i] > 0) || !std::isfinite(damage[i]) || damage[i] > std::numeric_limits<float>::max()) continue;
                AreaCast cast{{body->GetFormID(), static_cast<Type>(i), damage[i], allowed},
                    RE::NiPointer<RE::Actor>(body), RE::NiPointer<RE::Actor>(p)};
                struct Applying {
                    bool old = applying; AreaCast* previous = areaCast;
                    explicit Applying(AreaCast* cast) { applying = true; areaCast = cast; }
                    ~Applying() { applying = old; areaCast = previous; }
                } scope(&cast);
                SKSE::log::info("[CorpseExplosion] area-cast victim={:08X} type={} requested={} eligible={} origin=corpse visibility=native-area",
                    body->GetFormID(), names[i], damage[i], allowed.size());
                caster->CastSpellImmediate(spells[i], false, nullptr, 1.f, false, static_cast<float>(damage[i]), p);
                accepted += cast.delivery.accepted;
                SKSE::log::info("[CorpseExplosion] area-result victim={:08X} type={} attempted={} accepted={}",
                    body->GetFormID(), names[i], cast.delivery.attempted.size(), cast.delivery.accepted);
            }
            SKSE::log::info("[CorpseExplosion] burst victim={:08X} enemies={} accepted-portions={} radius={}; fire={} frost={} shock={} poison={}",
                body->GetFormID(), allowed.size(), accepted, radius, damage[0], damage[1], damage[2], damage[3]);
        }
        void pump(std::uint64_t generation) {
            if (generation != epoch.load()) return;
            // Keep the gate held while damage callbacks run. New chain kills
            // are pending for the next player update, never recursive casts.
            struct Completion {
                std::atomic_bool& queued;
                const std::atomic<std::uint64_t>& epoch;
                std::uint64_t generation;
                ~Completion() { if (epoch.load() == generation) queued.store(false); }
            } completion{taskQueued, epoch, generation};
            if (!selected()) return;
            std::vector<ID> pending;
            { std::lock_guard lock(mutex); for (const auto& [id, t] : ledger.targets)
                if (t.state == State::pending) pending.push_back(id); }
            unsigned bursts{};
            for (auto id : pending) {
                if (bursts >= maxBurstsPerWave || generation != epoch.load() || !selected()) break;
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
                if (damage && generation == epoch.load() && selected()) {
                    ++bursts;
                    explode(body.get(), *damage);
                }
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
        template<const auto& VTables> struct HealthHook {
            static_assert(VTables.size() >= 10, "Health hooks require an explicit actor vtable array");
            inline static REL::Relocation<void (*)(RE::Actor*, RE::Actor*, float)> original;
            static void Damage(RE::Actor* actor, RE::Actor* attacker, float damage) {
                Origin origin{attacker == RE::PlayerCharacter::GetSingleton(), applying};
                if (!attacker || origin.player)
                    for (auto f = active; f; f = f->parent) if (f->actor == actor->GetFormID()) { origin = f->origin; break; }
                Scope observed(actor, origin, true);
                original(actor, attacker, damage);
                observed.finish();
            }
            static void install(std::string_view name) {
                SKSE::log::info("[CorpseExplosion] installing {} HandleHealthDamage hook", name);
                REL::Relocation<std::uintptr_t> table{std::get<0>(VTables)};
                original = table.write_vfunc(0x104, Damage);
                SKSE::log::info("[CorpseExplosion] installed {} HandleHealthDamage hook", name);
            }
        };
    public:
        struct DamageSample {
            ID spell{}, caster{};
            unsigned type{};
            float magnitude{};
        };
        static DamageSample captureDamage(RE::ActiveEffect* effect, RE::Actor* actor) {
            DamageSample sample;
            if (!self || !effect || !effect->effect || !effect->spell ||
                !harvest::matchesMagicTarget(effect->target, actor)) return sample;
            const auto found = std::find(self->effects.begin(), self->effects.end(), effect->effect->baseEffect);
            if (found == self->effects.end()) return sample;
            sample.spell = effect->spell->GetFormID();
            sample.type = static_cast<unsigned>(found - self->effects.begin());
            sample.magnitude = effect->magnitude;
            const auto caster = effect->GetCasterActor();
            sample.caster = caster ? caster->GetFormID() : 0;
            return sample;
        }
        static void reportDamage(const DamageSample& sample, RE::Actor* actor, float value, float before, float after) {
            if (!sample.spell || !actor) return;
            // Captured values only: the ActiveEffect may have been destroyed
            // inside the native modification call. Also runs for later ticks.
            SKSE::log::info("[CorpseExplosion] health-update target={:08X} spell={:08X} caster={:08X} type={} effect-magnitude={} native-value={} Health {} -> {} actual-loss={}",
                actor->GetFormID(), sample.spell, sample.caster, names[sample.type], sample.magnitude,
                value, before, after, healthLost(before, after));
        }
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
                if (frame.origin.player && result.damage > 0 && runtime->diagnosticCount.fetch_add(1) < 100)
                    SKSE::log::info("[CorpseExplosion] damage victim={:08X} source={:08X} type={} chain={} actual={}",
                        frame.actor, frame.origin.poison, names[static_cast<unsigned>(frame.origin.type)], frame.origin.explosion, result.damage);
                if (offered) {
                    SKSE::log::info("[CorpseExplosion] confirmed {} killing blow victim={:08X} source={:08X} type={}",
                        result.death->explosion ? "chain explosion" : "poison/oil",
                        frame.actor, result.death->poison, names[static_cast<unsigned>(result.death->type)]);
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
            for (unsigned i = 0; i < 4; ++i) {
                const auto spell = spells[i];
                const auto effect = effects[i];
                const bool valid = spell->data.delivery == RE::MagicSystem::Delivery::kSelf &&
                    effect->data.delivery == RE::MagicSystem::Delivery::kSelf &&
                    effect->data.explosion == visuals[i] && spell->effects.size() == 1 &&
                    spell->effects[0] && spell->effects[0]->baseEffect == effect &&
                    spell->effects[0]->effectItem.area == radiusFeet && spell->effects[0]->effectItem.duration == 0 &&
                    !effect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kNoArea) &&
                    !spell->data.flags.any(RE::SpellItem::SpellFlag::kIgnoreLOSCheck) &&
                    spell->data.flags.any(RE::SpellItem::SpellFlag::kIgnoreResistance) &&
                    spell->data.flags.any(RE::SpellItem::SpellFlag::kNoAbsorb);
                if (!valid) {
                    ready = false;
                    SKSE::log::error("[CorpseExplosion] {} area records mismatched; requires Combined 2.13.7 ESP winning conflicts (native area LOS enabled)", names[i]);
                }
            }
            if (!ready) return;
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
            AreaTargetHook<RE::VTABLE_Actor>::install("Actor");
            AreaTargetHook<RE::VTABLE_Character>::install("Character");
            AreaTargetHook<RE::VTABLE_PlayerCharacter>::install("PlayerCharacter");
            HealthHook<RE::VTABLE_Actor>::install("Actor");
            HealthHook<RE::VTABLE_Character>::install("Character");
            REL::Relocation<std::uintptr_t> table{RE::VTABLE_PlayerCharacter[0]};
            originalUpdate = table.write_vfunc(0xAD, Update);
            SKSE::log::info("[CorpseExplosion] ready: corpse-origin Self-area spells, {} percent + {} flat damage, radius {} feet ({:.3f} units), native area LOS, matching resistance, enemy filter, chains enabled, {} bursts per wave; acceptance and Health diagnostics", fraction * 100, flatDamage, radiusFeet, radius, maxBurstsPerWave);
        }
        void setSession(bool value) { session.store(value); }
        void reset() {
            ++epoch; taskQueued.store(false); diagnosticCount.store(0); maintenance = 0;
            std::lock_guard lock(mutex); ledger.clear(); wasSelected = false;
        }
        void forget(ID id) { std::lock_guard lock(mutex); ledger.forget(id); }
        void save(SKSE::SerializationInterface* api) {
            std::lock_guard lock(mutex); const auto bytes = encode(ledger);
            if (!api->WriteRecord(recordID, 2, bytes.data(), static_cast<std::uint32_t>(bytes.size())))
                SKSE::log::error("[CorpseExplosion] save failed");
        }
        void load(SKSE::SerializationInterface* api, std::uint32_t version, std::uint32_t size) {
            if ((version != 1 && version != 2) || size > 32 * 1024 * 1024) return;
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
