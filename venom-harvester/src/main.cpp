#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "Harvest.h"
#include "CraftCapture.h"
#include "MenuGate.h"
#include <array>
#include <atomic>
#include <cmath>
#include <mutex>

namespace
{
    constexpr auto pluginFile = "Biggie Traits - Combined.esp";
    constexpr std::uint32_t saveID = 0x56484D31; // Preserve the old plugin's SKSE namespace.
    constexpr std::uint32_t recordID = 0x48534154; // HSAT; old HARV refunds are not imported.
    RE::SpellItem *trait{}, *power{}, *weakness{};
    RE::EffectSetting* batchMarker{};
    RE::BGSListForm* retained{};
    RE::IngredientItem* jarrin{};
    std::atomic_bool ready{}, session{}, queued{};
    harvest::MenuGate menuGate;
    std::atomic<std::uint64_t> epoch{};
    std::atomic_uint damageDiagnostics{};
    std::mutex mutex;
    harvest::Ledger ledger;

    bool selected()
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        return ready.load() && session.load() && player && player->HasSpell(trait);
    }
    void queueWork();

    std::uint32_t nonceOf(RE::AlchemyItem* item)
    {
        if (!item) return 0;
        for (const auto e : item->effects) {
            if (!e || e->baseEffect != batchMarker) continue;
            const float n = e->effectItem.magnitude;
            if (std::isfinite(n) && n >= 1 && n <= harvest::nonceLimit && n == std::floor(n))
                return static_cast<std::uint32_t>(n);
        }
        return 0;
    }

    // CommonLib powerof3's documented created-object API, adapted to the
    // pinned CommonLibSSE-NG ABI. No raw TESForm duplication or borrowed effects.
    template<class T> struct CreatedPolicy
    {
        static void Acquire(T* p)
        {
            auto manager = RE::BGSCreatedObjectManager::GetSingleton();
            if (!manager || !p || (p->GetFormID() >> 24) != 0xFF) return;
            using Fn = void(RE::BGSCreatedObjectManager*, RE::AlchemyItem*);
            static REL::Relocation<Fn*> fn{RELOCATION_ID(35268, 36170)};
            fn(manager, p);
        }
        static void Release(T* p)
        {
            auto manager = RE::BGSCreatedObjectManager::GetSingleton();
            if (!manager || !p || (p->GetFormID() >> 24) != 0xFF) return;
            using Fn = void(RE::BGSCreatedObjectManager*, RE::AlchemyItem*);
            static REL::Relocation<Fn*> fn{RELOCATION_ID(35269, 36171)};
            fn(manager, p);
        }
    };
    using CreatedPoison = RE::BSTSmartPointer<RE::AlchemyItem, CreatedPolicy>;

    CreatedPoison makeBatch(RE::AlchemyItem* original, std::uint32_t nonce)
    {
        CreatedPoison result;
        auto manager = RE::BGSCreatedObjectManager::GetSingleton();
        if (!manager || !original || !original->IsPoison() || nonceOf(original) || original->effects.empty()) return result;
        RE::BSTArray<RE::Effect> effects;
        // A freshly brewed vanilla-menu poison has unconditional EFIT entries.
        // Fail closed for a custom conditional entry rather than discard it.
        for (auto e : original->effects) {
            if (!e || !e->baseEffect || e->conditions.head) return {};
            RE::Effect copy;
            copy.effectItem = e->effectItem;
            copy.baseEffect = e->baseEffect;
            copy.cost = e->cost;
            effects.push_back(copy);
        }
        RE::Effect marker;
        marker.baseEffect = batchMarker;
        marker.effectItem.magnitude = static_cast<float>(nonce);
        effects.push_back(marker); // Hidden, zero cost, empty Script archetype.
        using Fn = void(RE::BGSCreatedObjectManager*, CreatedPoison&, RE::BSTArray<RE::Effect>&);
        static REL::Relocation<Fn*> create{RELOCATION_ID(35265, 36167)};
        create(manager, result, effects);
        if (!result || !result->IsPoison() || result.get() == original || (result->GetFormID() >> 24) != 0xFF ||
            retained->HasForm(result.get()) || nonceOf(result.get()) != nonce ||
            result->effects.size() != original->effects.size() + 1) return {};
        // Verify every real effect survived native creation exactly once.
        std::vector<RE::Effect*> remaining;
        for (auto e : result->effects) if (e && e->baseEffect != batchMarker) remaining.push_back(e);
        for (auto e : original->effects) {
            const auto it = std::find_if(remaining.begin(), remaining.end(), [e](RE::Effect* other) {
                return e->baseEffect == other->baseEffect &&
                    e->effectItem.magnitude == other->effectItem.magnitude &&
                    e->effectItem.duration == other->effectItem.duration &&
                    e->effectItem.area == other->effectItem.area;
            });
            if (it == remaining.end()) return {};
            remaining.erase(it);
        }
        result->data = original->data;
        result->fullName = original->fullName;
        result->weight = original->weight;
        return result->IsPoison() ? result : CreatedPoison{};
    }

    using AlchemyMenu = RE::CraftingSubMenus::CraftingSubMenus::AlchemyMenu;
    struct Craft;
    thread_local Craft* crafting{};
    thread_local const char* captureStatus = "no alchemy callback captured";

    struct Craft
    {
        Craft* previous = crafting;
        std::uint64_t generation = epoch.load();
        harvest::Recipe recipe;
        harvest::InventoryCounts before;
        std::vector<RE::FormID> craftEvents;
        bool active{};

        explicit Craft(AlchemyMenu* menu)
        {
            if (previous || !menu || !selected()) return;
            captureStatus = "no stored recipe and recording is not armed";
            {
                std::lock_guard lock(mutex);
                if (!ledger.armed && ledger.stored.empty()) return;
            }
            for (auto index : menu->selectedIndexes) {
                captureStatus = "selected ingredient index is invalid";
                if (index >= menu->ingredientEntries.size()) return;
                const auto entry = menu->ingredientEntries[index].ingredient;
                auto object = entry ? entry->object : nullptr;
                captureStatus = "selected item is not an ingredient";
                if (!object || !object->As<RE::IngredientItem>()) return;
                recipe.push_back(object->GetFormID());
            }
            std::sort(recipe.begin(), recipe.end());
            captureStatus = "recipe does not contain two or three distinct ingredients";
            if (!harvest::validRecipe(recipe)) return;
            {
                std::lock_guard lock(mutex);
                captureStatus = "recipe differs from the remembered ingredients";
                if (!ledger.armed && recipe != ledger.stored) return;
            }
            const auto player = RE::PlayerCharacter::GetSingleton();
            for (const auto& [item, count] : player->GetInventoryCounts()) {
                if (item && (item->As<RE::IngredientItem>() || item->As<RE::AlchemyItem>()))
                    before[item->GetFormID()] = count;
            }
            active = true;
            crafting = this;
            captureStatus = "captured";
        }
        ~Craft()
        {
            if (!active) return;
            // Keep this transaction installed through the inventory exchange;
            // nested UI/item callbacks cannot start a second crafting budget.
            finish();
            crafting = previous;
        }
        void finish()
        {
            if (craftEvents.empty()) return; // Normal non-crafting menu callbacks.
            if (generation != epoch.load() || !selected()) return;
            if (craftEvents.size() != 1) {
                SKSE::log::warn("Craft rejected: {} distinct poison craft event forms in one callback", craftEvents.size());
                return;
            }
            const auto player = RE::PlayerCharacter::GetSingleton();
            if (!player) return;
            harvest::InventoryCounts after;
            for (const auto& [item, count] : player->GetInventoryCounts())
                if (item && (item->As<RE::IngredientItem>() || item->As<RE::AlchemyItem>())) after[item->GetFormID()] = count;
            const auto output = harvest::identifyCraftOutput(!craftEvents.empty(), before, after, [](harvest::ID id) {
                const auto item = RE::TESForm::LookupByID<RE::AlchemyItem>(id);
                return item && item->IsPoison() && !nonceOf(item);
            });
            if (output.status != harvest::OutputStatus::found) {
                SKSE::log::warn("Craft event {:08X} rejected: {}", craftEvents.front(), harvest::outputStatusName(output.status));
                for (const auto& [id, count] : after) {
                    const auto oldCount = harvest::inventoryCount(before, id);
                    if (count > oldCount) if (const auto item = RE::TESForm::LookupByID<RE::AlchemyItem>(id))
                        SKSE::log::info("Craft inventory diagnostic: {:08X}; {} -> {}; poison {}; marker {}",
                            id, oldCount, count, item->IsPoison(), nonceOf(item));
                }
                RE::DebugNotification("The Satchel could not record this batch.");
                return;
            }
            auto original = RE::TESForm::LookupByID<RE::AlchemyItem>(output.item);
            if (!original) return;
            const auto bottles = output.bottles;
            SKSE::log::info("Craft event {:08X}; actual output {:08X}; net {} bottles",
                craftEvents.front(), output.item, bottles);
            const auto consumed = harvest::consumedIngredients(recipe, before, after);
            if (!consumed) {
                SKSE::log::warn("Craft rejected: invalid ingredient expenditure");
                RE::DebugNotification("The Satchel could not record this batch.");
                return;
            }
            const auto& cost = *consumed;
            bool remembered = false;
            std::uint32_t nonce = 0;
            {
                std::lock_guard lock(mutex);
                if (ledger.armed) remembered = ledger.remember(recipe);
                if (ledger.stored != recipe) return;
                if (harvest::validCost(cost, recipe) && ledger.sequence < harvest::nonceLimit) nonce = ++ledger.sequence;
            }
            if (!nonce) {
                SKSE::log::info("Recipe recorded={}; no refundable batch: {} consumed ingredient types, nonce unavailable",
                    remembered, cost.size());
                if (remembered) RE::DebugNotification("The Satchel remembers the recipe, but this batch has no refund recorded.");
                return; // A completely free craft has no ingredients to refund.
            }
            auto unique = makeBatch(original, nonce);
            if (!unique) {
                SKSE::log::warn("Batch {}: native poison identity check failed; inventory left intact", nonce);
                RE::DebugNotification("The Satchel could not bind this batch.");
                return;
            }
            const auto poisonID = unique->GetFormID();
            {
                std::lock_guard lock(mutex);
                if (!ledger.add({poisonID, nonce, recipe, cost, false})) return;
            }
            // Only the verified net output of this exact crafting callback is
            // exchanged. Pre-existing purchased/brewed bottles remain unbound.
            retained->AddForm(unique.get());
            player->RemoveItem(original, bottles, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            player->AddObjectToContainer(unique.get(), nullptr, bottles, nullptr);
            if (remembered) RE::DebugNotification("Huntsman's Satchel recorded this poison's recipe and batch.");
            SKSE::log::info("Bound batch {}: {:08X} -> {:08X}; {} bottles; {} ingredient types", nonce,
                original->GetFormID(), poisonID, bottles, cost.size());
            for (const auto& part : cost)
                SKSE::log::info("Batch {} ingredient {:08X}: consumed {}", nonce, part.form, part.count);
        }
    };

    // Cover mouse, keyboard/controller and the game's asynchronous confirmation
    // box. Every wrapper preserves the original callback and ignores non-crafts.
    struct CraftHooks
    {
        using Callback = RE::FxDelegateHandler::CallbackFn;
        using Processor = RE::FxDelegateHandler::CallbackProcessor;
        inline static std::array<Callback*, 64> callbacks{};
        inline static std::size_t count{};
        inline static REL::Relocation<void (*)(AlchemyMenu*, Processor*)> accept;
        inline static REL::Relocation<bool (*)(AlchemyMenu*, RE::BSFixedString*)> userEvent;
        inline static REL::Relocation<void (*)(RE::IMessageBoxCallback*, RE::IMessageBoxCallback::Message)> confirm;

        template<std::size_t I> static void call(const RE::FxDelegateArgs& args)
        {
            Craft transaction(static_cast<AlchemyMenu*>(args.GetHandler()));
            callbacks[I](args);
        }
        template<std::size_t... I> static constexpr auto wrappers(std::index_sequence<I...>)
        {
            return std::array<Callback*, sizeof...(I)>{&call<I>...};
        }
        class Proxy final : public Processor
        {
            Processor* real;
        public:
            explicit Proxy(Processor* value) : real(value) {}
            void Process(const RE::GString& name, Callback* callback) override
            {
                static constexpr auto functions = wrappers(std::make_index_sequence<64>{});
                std::size_t index = 0;
                while (index < count && callbacks[index] != callback) ++index;
                if (index == count && count < callbacks.size()) callbacks[count++] = callback;
                real->Process(name, index < callbacks.size() ? functions[index] : callback);
            }
        };
        static void Accept(AlchemyMenu* menu, Processor* processor)
        {
            Proxy proxy(processor); accept(menu, &proxy);
        }
        static bool UserEvent(AlchemyMenu* menu, RE::BSFixedString* control)
        {
            Craft transaction(menu); return userEvent(menu, control);
        }
        static void Confirm(RE::IMessageBoxCallback* callback, RE::IMessageBoxCallback::Message button)
        {
            // CraftItemCallback's AlchemyMenu* is at 0x10 (CommonLib ABI).
            auto menu = *reinterpret_cast<AlchemyMenu**>(reinterpret_cast<std::uintptr_t>(callback) + 0x10);
            Craft transaction(menu); confirm(callback, button);
        }
        static void install()
        {
            REL::Relocation<std::uintptr_t> table{RE::VTABLE_CraftingSubMenus__AlchemyMenu[0]};
            accept = table.write_vfunc(0x01, Accept);
            userEvent = table.write_vfunc(0x05, UserEvent);
            REL::Relocation<std::uintptr_t> confirmation{RE::VTABLE_CraftingSubMenus__AlchemyMenu__CraftItemCallback[0]};
            confirm = confirmation.write_vfunc(0x01, Confirm);
        }
    };

    bool ownedPoison(RE::ActiveEffect* effect, RE::Actor* target)
    {
        if (!effect || !target || !effect->spell || !effect->effect || !effect->effect->baseEffect) return false;
        const auto base = effect->effect->baseEffect;
        if ((!base->IsHostile() && !base->IsDetrimental()) || base == batchMarker) return false;
        const auto poison = effect->spell->As<RE::AlchemyItem>();
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!poison || !poison->IsPoison() || !player || target == player ||
            effect->GetTargetActor() != target || effect->GetCasterActor().get() != player || !selected()) return false;
        if (effect->flags.any(RE::ActiveEffect::Flag::kDispelled) ||
            effect->conditionStatus == RE::ActiveEffect::ConditionStatus::kFalse) return false;
        const auto nonce = nonceOf(poison);
        std::lock_guard lock(mutex);
        const auto found = ledger.batches.find(poison->GetFormID());
        return nonce && found != ledger.batches.end() && found->second.nonce == nonce && ledger.eligible(poison->GetFormID());
    }

    // Observe the native value-change operation itself. There is deliberately
    // no TESDeathEvent scan of active poisons and no outgoing-damage modifier.
    thread_local std::uint64_t lethalSerial{};
    template<class T> struct DamageHook
    {
        inline static REL::Relocation<void (*)(RE::ValueModifierEffect*, RE::Actor*, float, RE::ActorValue)> original;
        static void Modify(RE::ValueModifierEffect* effect, RE::Actor* actor, float value, RE::ActorValue av)
        {
            const auto generation = epoch.load(), serial = lethalSerial;
            const bool healthChange = av == RE::ActorValue::kHealth;
            const bool alive = actor && actor->AsActorState()->GetLifeState() == RE::ACTOR_LIFE_STATE::kAlive;
            const float health = alive ? actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) : 0;
            const bool eligible = healthChange && alive && health > 0 && ownedPoison(effect, actor);
            // The ActiveEffect can be deleted inside the engine's call.
            const auto source = eligible ? effect->spell->GetFormID() : 0;
            RE::FormID diagnosticSource = 0;
            std::uint32_t diagnosticNonce = 0;
            if (healthChange && actor && effect && effect->spell && selected() && damageDiagnostics.load() < 80) {
                const auto poison = effect->spell->As<RE::AlchemyItem>();
                if (poison && poison->IsPoison() && effect->GetCasterActor().get() == RE::PlayerCharacter::GetSingleton()) {
                    if (damageDiagnostics.fetch_add(1) < 80) {
                        diagnosticSource = poison->GetFormID();
                        diagnosticNonce = nonceOf(poison);
                    }
                }
            }
            const RE::NiPointer<RE::Actor> keepAlive(actor);
            original(effect, actor, value, av);
            if (!actor || !healthChange) return;
            const float after = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
            const auto state = actor->AsActorState()->GetLifeState();
            if (diagnosticSource) SKSE::log::info(
                "Poison Health change: source {:08X}; batch {}; victim {:08X}; alive-before {}; health {} -> {}; state {}; eligible {}",
                diagnosticSource, diagnosticNonce, actor->GetFormID(), alive, health, after, static_cast<unsigned>(state), eligible);
            if (!alive || health <= 0 || serial != lethalSerial || after > 0) return;
            if (state != RE::ACTOR_LIFE_STATE::kDying && state != RE::ACTOR_LIFE_STATE::kDead) return;
            ++lethalSerial; // An unbound poison can also block outer attribution.
            if (!eligible || generation != epoch.load() || !selected()) return;
            bool offered;
            {
                std::lock_guard lock(mutex);
                offered = ledger.offer(actor->GetFormID(), source);
            }
            if (offered) {
                SKSE::log::info("Poison lethal: source {:08X}; victim {:08X}", source, actor->GetFormID());
                queueWork();
            }
        }
        static void install()
        {
            REL::Relocation<std::uintptr_t> table{T::VTABLE[0]};
            original = table.write_vfunc(0x20, Modify);
        }
    };

    class Choice final : public RE::IMessageBoxCallback
    {
        const harvest::MenuRequest request;
        std::atomic_bool submitted{};
    public:
        explicit Choice(harvest::MenuRequest value) : request(value) { unk0C = 0; }
        void Run(Message button) override
        {
            if (submitted.exchange(true)) return;
            const auto captured = request;
            const auto index = static_cast<unsigned>(button);
            SKSE::log::info("Satchel menu: callback ticket={}, button={}", captured.ticket, index);
            // The menu owns this object. Copy only values into the game task;
            // never retain this or call actor/inventory APIs from the UI callback.
            if (auto tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([captured, index] {
                    const auto recording = menuGate.resolve(captured, epoch.load(), index, selected());
                    if (!recording.has_value()) return;
                    {
                        std::lock_guard lock(mutex);
                        ledger.armed = *recording;
                    }
                    SKSE::log::info("Satchel menu: recording={}", *recording);
                    RE::DebugNotification(*recording ?
                        "Brew a poison to teach its recipe to the Satchel." :
                        "The Satchel stops waiting for a new recipe.");
                });
            } else {
                menuGate.finish(captured.ticket);
                SKSE::log::error("Satchel menu: task interface unavailable");
            }
        }
    };

    void showPower()
    {
        if (!selected()) return;
        const auto ticket = menuGate.begin();
        if (!ticket) return;
        auto factories = RE::MessageDataFactoryManager::GetSingleton();
        auto strings = RE::InterfaceStrings::GetSingleton();
        auto factory = factories && strings ? factories->GetCreator<RE::MessageBoxData>(strings->messageBoxData) : nullptr;
        auto box = factory ? factory->Create() : nullptr;
        if (!box) {
            menuGate.finish(ticket);
            SKSE::log::error("Satchel menu: message-box factory unavailable");
            return;
        }
        harvest::Recipe recipe;
        bool armed;
        std::size_t unclaimed;
        {
            std::lock_guard lock(mutex);
            recipe = ledger.stored; armed = ledger.armed;
            unclaimed = ledger.unclaimed();
        }
        std::string body = "Huntsman's Satchel\n\n";
        if (recipe.empty()) body += "No poison recipe remembered.\n";
        else {
            body += "Remembered ingredients:\n";
            for (auto id : recipe) {
                const auto ingredient = RE::TESForm::LookupByID<RE::IngredientItem>(id);
                body += ingredient ? ingredient->GetName() : "Missing ingredient";
                body += "\n";
            }
            body += "\nUnclaimed crafting batches: " + std::to_string(unclaimed) + "\n";
        }
        body += armed ? "\nWaiting for the next poison you brew." :
            "\nA killing blow from this poison returns the ingredients spent on its batch, once. Remember a new recipe by brewing it.";
        // Do NOT pass an IMessageBoxCallback object to RE::CreateMessage.
        // Its pinned CommonLib signature is misleading: the native helper
        // expects an old-style raw function pointer and null-ended varargs.
        // MessageBoxData's smart callback field is the actual object API.
        static_assert(offsetof(RE::MessageBoxData, callback) == 0x40);
        static_assert(offsetof(RE::MessageBoxData, unk4C) == 0x4C);
        box->bodyText = body.c_str();
        box->buttonText.push_back(armed ? "Cancel recording" : "Remember next poison");
        box->buttonText.push_back("Close");
        box->unk3C = 1; // cancelButtonIndex: Close
        box->unk4C = 0; // buttonPressOffset
        box->unk4F = 1; // isCancellable
        box->callback = RE::make_smart<Choice>(harvest::MenuRequest{epoch.load(), ticket, armed});
        SKSE::log::info("Satchel menu: opened ticket={}, recording={}, unclaimed batches={}", ticket, armed, unclaimed);
        box->QueueMessage();
    }

    void syncTrait()
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;
        const bool enabled = selected();
        for (auto spell : {power, weakness}) {
            if (enabled && !player->HasSpell(spell)) player->AddSpell(spell);
            else if (!enabled && player->HasSpell(spell)) player->RemoveSpell(spell);
        }
        if (!enabled) {
            std::lock_guard lock(mutex); ledger.armed = false; ledger.candidates.clear();
            return;
        }
        bool give = false;
        {
            std::lock_guard lock(mutex);
            // A saved FLST marker protects the once-per-character gift even if
            // the SKSE co-save is lost. It has no inventory or combat effect.
            if (retained->HasForm(jarrin)) ledger.giftGiven = true;
            if (!ledger.giftGiven) { ledger.giftGiven = true; give = true; }
        }
        if (give) {
            retained->AddForm(jarrin); // Claim before AddObject can re-enter.
            player->AddObjectToContainer(jarrin, nullptr, 1, nullptr);
            SKSE::log::info("Granted the character's one Jarrin Root");
        }
    }

    void flush(std::uint64_t generation)
    {
        if (generation != epoch.load()) return;
        queued.store(false);
        if (!ready.load() || !session.load()) return;
        syncTrait();
        if (!selected()) return;
        std::vector<harvest::ID> victims;
        {
            std::lock_guard lock(mutex);
            for (const auto& [actor, poison] : ledger.candidates) victims.push_back(actor);
        }
        for (auto id : victims) {
            std::optional<harvest::Ingredients> reward;
            {
                std::lock_guard lock(mutex);
                if (generation != epoch.load()) return;
                reward = ledger.claimVerified(id, [](harvest::ID ingredient) {
                    return RE::TESForm::LookupByID<RE::IngredientItem>(ingredient) != nullptr;
                });
            }
            if (!reward) continue;
            const auto player = RE::PlayerCharacter::GetSingleton();
            for (const auto& part : *reward) player->AddObjectToContainer(
                RE::TESForm::LookupByID<RE::IngredientItem>(part.form), nullptr, static_cast<std::int32_t>(part.count), nullptr);
            RE::DebugNotification("Huntsman's Satchel returns your poison's ingredients.");
            SKSE::log::info("Refunded {} ingredient types for victim {:08X}", reward->size(), id);
        }
    }
    void queueWork()
    {
        if (!ready.load() || !session.load() || queued.exchange(true)) return;
        const auto generation = epoch.load();
        SKSE::GetTaskInterface()->AddTask([generation] { flush(generation); });
    }
    void poll(RE::StaticFunctionTag*) { queueWork(); }

    class Events final : public RE::BSTEventSink<RE::ItemCrafted::Event>,
                         public RE::BSTEventSink<RE::TESSpellCastEvent>,
                         public RE::BSTEventSink<RE::TESFormDeleteEvent>
    {
    public:
        using Result = RE::BSEventNotifyControl;
        static Events& get() { static Events value; return value; }
        Result ProcessEvent(const RE::ItemCrafted::Event* event, RE::BSTEventSource<RE::ItemCrafted::Event>*) override
        {
            if (!event || !event->item || !selected()) return Result::kContinue;
            auto item = event->item->As<RE::AlchemyItem>();
            if (item && item->IsPoison()) {
                if (crafting) {
                    const auto id = item->GetFormID();
                    if (std::find(crafting->craftEvents.begin(), crafting->craftEvents.end(), id) == crafting->craftEvents.end())
                        crafting->craftEvents.push_back(id);
                }
                else SKSE::log::info("Poison craft outside a captured alchemy transaction: {:08X}; {}; no refund budget",
                    item->GetFormID(), captureStatus);
            }
            return Result::kContinue;
        }
        Result ProcessEvent(const RE::TESSpellCastEvent* event, RE::BSTEventSource<RE::TESSpellCastEvent>*) override
        {
            if (event && event->object.get() == RE::PlayerCharacter::GetSingleton() &&
                power && event->spell == power->GetFormID() && selected()) {
                const auto generation = epoch.load();
                SKSE::GetTaskInterface()->AddTask([generation] { if (generation == epoch.load()) showPower(); });
            }
            return Result::kContinue;
        }
        Result ProcessEvent(const RE::TESFormDeleteEvent* event, RE::BSTEventSource<RE::TESFormDeleteEvent>*) override
        {
            if (event) { std::lock_guard lock(mutex); ledger.forgetActor(event->formID); }
            return Result::kContinue;
        }
    };

    void reset()
    {
        ++epoch; queued.store(false); menuGate.reset(); damageDiagnostics.store(0);
        std::lock_guard lock(mutex); ledger.clear();
    }
    void save(SKSE::SerializationInterface* api)
    {
        std::lock_guard lock(mutex);
        const auto bytes = harvest::encode(ledger);
        if (!api->WriteRecord(recordID, 2, bytes.data(), static_cast<std::uint32_t>(bytes.size())))
            SKSE::log::error("Could not write Huntsman's Satchel save state");
    }
    void load(SKSE::SerializationInterface* api)
    {
        reset();
        std::uint32_t type, version, length;
        while (api->GetNextRecordInfo(type, version, length)) {
            if (type != recordID || version != 2 || length > 64 * 1024 * 1024) continue;
            std::vector<std::uint8_t> bytes(length);
            if (api->ReadRecordData(bytes.data(), length) != length) continue;
            auto state = harvest::decode(bytes, [api](harvest::ID oldID) {
                RE::FormID id = 0; return api->ResolveFormID(oldID, id) ? id : 0;
            });
            if (state) {
                std::lock_guard lock(mutex); ledger = std::move(*state);
                SKSE::log::info("Restored {} crafting batches", ledger.batches.size());
            } else SKSE::log::error("Rejected invalid Satchel state; no inferred refunds");
        }
    }
    void resume()
    {
        if (!ready.load()) return;
        // Also account for saved marked poisons when the co-save was missing.
        std::uint32_t maximum = 0;
        retained->ForEachForm([&](RE::TESForm* form) {
            if (form) maximum = std::max(maximum, nonceOf(form->As<RE::AlchemyItem>()));
            return RE::BSContainer::ForEachResult::kContinue;
        });
        {
            std::lock_guard lock(mutex); ledger.sequence = std::max(ledger.sequence, maximum);
        }
        queueWork();
    }
    void onMessage(SKSE::MessagingInterface::Message* message)
    {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            const auto data = RE::TESDataHandler::GetSingleton();
            trait = data->LookupForm<RE::SpellItem>(0xA00, pluginFile);
            retained = data->LookupForm<RE::BGSListForm>(0xA06, pluginFile);
            power = data->LookupForm<RE::SpellItem>(0xF40, pluginFile);
            weakness = data->LookupForm<RE::SpellItem>(0xF42, pluginFile);
            batchMarker = data->LookupForm<RE::EffectSetting>(0xF44, pluginFile);
            jarrin = data->LookupForm<RE::IngredientItem>(0x1BCBC, "Skyrim.esm");
            ready.store(trait && retained && power && weakness && batchMarker && jarrin);
            if (!ready.load()) { SKSE::log::error("Requires Biggie Traits Combined 2.8.0 or later Satchel records"); return; }
            const auto source = RE::ScriptEventSourceHolder::GetSingleton();
            source->AddEventSink<RE::TESSpellCastEvent>(&Events::get());
            source->AddEventSink<RE::TESFormDeleteEvent>(&Events::get());
            RE::ItemCrafted::GetEventSource()->AddEventSink(&Events::get());
            CraftHooks::install();
            DamageHook<RE::ValueModifierEffect>::install();
            DamageHook<RE::DualValueModifierEffect>::install();
            DamageHook<RE::PeakValueModifierEffect>::install();
            DamageHook<RE::AccumulatingValueModifierEffect>::install();
            DamageHook<RE::AbsorbEffect>::install();
            SKSE::log::info("Ready: Huntsman's Satchel; PoisonResist -50; outgoing poisons unchanged");
        } else if (message->type == SKSE::MessagingInterface::kPreLoadGame) {
            session.store(false); reset();
        } else if (message->type == SKSE::MessagingInterface::kNewGame) {
            reset(); session.store(true); resume();
        } else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
            session.store(message->data != nullptr); if (session.load()) resume();
        }
    }
}

extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data{};
    data.PluginVersion({2, 0, 3, 0}); data.PluginName("VenomHarvester");
    data.AuthorName("Physics-helper contributors");
    data.UsesAddressLibrary(true); data.UsesStructsPost629(true);
    data.CompatibleVersions({REL::Version{1, 6, 1170, 0}});
    return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse)
{
    if (skse->RuntimeVersion() != REL::Version{1, 6, 1170, 0}) return false;
    auto path = SKSE::log::log_directory(); if (!path) return false;
    *path /= "VenomHarvester.log";
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("global",
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true)));
    spdlog::set_level(spdlog::level::info); spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    SKSE::log::info("Huntsman's Satchel 2.0.3 beta; Skyrim 1.6.1170; actual crafted inventory output capture; one ingredient refund per crafting batch");
    const auto serialization = SKSE::GetSerializationInterface();
    serialization->SetUniqueID(saveID); serialization->SetSaveCallback(save); serialization->SetLoadCallback(load);
    serialization->SetRevertCallback([](SKSE::SerializationInterface*) { session.store(false); reset(); });
    if (!SKSE::GetPapyrusInterface()->Register([](RE::BSScript::IVirtualMachine* vm) {
        vm->RegisterFunction("Poll", "VH_Native", poll); return true;
    })) return false;
    return SKSE::GetMessagingInterface()->RegisterListener(onMessage);
}
