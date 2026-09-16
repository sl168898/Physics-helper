// SPDX-License-Identifier: GPL-3.0-or-later
#include "FlailDirectSMP.h"
#include "ActorManager.h"
#include "hdtSkyrimPhysicsWorld.h"

#include <cctype>

namespace hdt
{
	namespace
	{
		constexpr auto physicsPath = "meshes\\GildyCreations\\GildFlailPhysics.xml";
		constexpr auto companionPlugin = "GildFlail_DirectSMP_Lorerim.esp";

		struct NodeTraits
		{
			static std::string_view name(RE::NiNode* node)
			{
				const auto p = node->name.c_str();
				return p ? std::string_view(p) : std::string_view{};
			}
			static bool hidden(RE::NiNode* node) { return node->GetAppCulled(); }
			template <class F>
			static void children(RE::NiNode* node, F&& f)
			{
				for (const auto& child : node->GetChildren()) {
					if (child) {
						if (auto n = child->AsNode()) {
							f(n);
						}
					}
				}
			}
		};
	}

	FlailDirectSMP& FlailDirectSMP::get()
	{
		static FlailDirectSMP manager;
		return manager;
	}

	void FlailDirectSMP::configure()
	{
		const auto requested = GetPrivateProfileIntA("FlailDirectSMP", "Enabled", 0,
			".\\Data\\SKSE\\Plugins\\FlailDirectSMP.ini") != 0;
		auto data = RE::TESDataHandler::GetSingleton();
		// The companion removes armor-equipping scripts and supplies this marker.
		const auto marker = data ? data->LookupForm<RE::BGSListForm>(0x800, companionPlugin) : nullptr;
		enabled = requested && marker;
		logger::info("[FlailDirectSMP] experimental prototype p2 / FSMP 3.2.1; enabled={}, requested={}, companion={}",
			enabled, requested, marker != nullptr);
	}

	void FlailDirectSMP::beginLoad()
	{
		loading = true;
		clear();
	}

	void FlailDirectSMP::endLoad(bool succeeded)
	{
		clear();
		loading = !succeeded;
	}

	void FlailDirectSMP::shutdown()
	{
		stopped = true;
		clear();
	}

	bool FlailDirectSMP::hasActiveSystems() const
	{
		return std::any_of(entries.begin(), entries.end(), [](const auto& item) {
			return item.second.system && item.second.system->m_world;
		});
	}

	void FlailDirectSMP::detach(Entry& entry)
	{
		if (entry.system && entry.system->m_world) {
			SkyrimPhysicsWorld::get()->removeSkinnedMeshSystem(entry.system.get());
		}
		entry.system = nullptr;
	}

	void FlailDirectSMP::clear()
	{
		if (entries.empty()) {
			return;
		}
		// FSMP can simulate asynchronously. Its world mutex alone does not make
		// it safe to release scene nodes that a queued second-step task still uses.
		SkyrimPhysicsWorld::get()->m_tasks.wait();
		for (auto& [node, entry] : entries) {
			detach(entry);
		}
		entries.clear();
	}

	bool FlailDirectSMP::legacyDriverPresent(RE::NiNode* actorRoot)
	{
		for (auto& skeleton : ActorManager::instance()->getSkeletons()) {
			if (skeleton.skeleton.get() != actorRoot) {
				continue;
			}
			for (const auto& armor : skeleton.getArmors()) {
				if (!armor.hasPhysics()) {
					continue;
				}
				auto path = armor.physicsFile.first;
				std::transform(path.begin(), path.end(), path.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (path.ends_with("gildflailphysics.xml")) {
					return true;
				}
			}
		}
		return false;
	}

	FlailDirectSMP::Entry FlailDirectSMP::attach(const Rig& rig)
	{
		Entry entry;
		entry.rig = rig;
		for (auto node : rig.attachment) {
			entry.heldNodes.push_back(make_nismart(node));
		}
		for (auto node : rig.bones) {
			entry.heldNodes.push_back(make_nismart(node));
		}

		DefaultBBP::PhysicsFile_t file{ physicsPath, {} };
		// The crucial change: the root is THIS flail's Flail_0, never the actor.
		// Both name lookup and the resulting system stay within that instance.
		// No NIF names, bones, meshes, equip slots, or armor records are mutated.
		auto system = SkyrimSystemCreator().createOrUpdateSystem(
			rig.bones[0], rig.bones[0], &file, {}, nullptr);
		bool complete = system && system->getBones().size() == flail::boneNames.size();
		if (complete) {
			for (std::size_t i = 0; i < rig.bones.size(); ++i) {
				auto bone = system->findBone(flail::boneNames[i].data());
				if (!bone || static_cast<SkyrimBone*>(bone)->m_node != rig.bones[i]) {
					complete = false;
					break;
				}
			}
		}
		if (!complete) {
			logger::warn("[FlailDirectSMP] instance {:p}: five-bone binding failed; leaving it unmanaged",
				static_cast<void*>(rig.bones[0]));
			return entry;
		}
		entry.system = system;
		SkyrimPhysicsWorld::get()->addSkinnedMeshSystem(system.get());
		logger::info("[FlailDirectSMP] bound independent instance {:p}, five bones",
			static_cast<void*>(rig.bones[0]));
		return entry;
	}

	void FlailDirectSMP::update()
	{
		if (!enabled || loading || stopped) {
			return;
		}
		auto world = SkyrimPhysicsWorld::get();
		world->m_tasks.wait();
		auto player = RE::PlayerCharacter::GetSingleton();
		auto object = player ? player->Get3D(false) : nullptr;
		auto root = object ? object->AsNode() : nullptr;
		if (!root || !root->parent || world->disabled) {
			clear();
			return;
		}
		if (legacyDriverPresent(root)) {
			clear();
			if (!warnedLegacy) {
				logger::warn("[FlailDirectSMP] legacy armor physics is present; direct binding suspended");
				warnedLegacy = true;
			}
			return;
		}
		warnedLegacy = false;

		auto scan = flail::collect<RE::NiNode, NodeTraits>(root);
		if (!scan.complete) {
			clear();
			if (!warnedLimit) {
				logger::warn("[FlailDirectSMP] scene traversal limit reached; direct binding suspended");
				warnedLimit = true;
			}
			return;
		}
		warnedLimit = false;

		std::vector<Rig> previous;
		previous.reserve(entries.size());
		for (const auto& [node, entry] : entries) {
			previous.push_back(entry.rig);
		}
		auto delta = flail::changes(previous, scan.rigs);
		// Always detach obsolete bindings before creating replacement systems.
		for (auto node : delta.remove) {
			auto old = entries.find(node);
			detach(old->second);
			entries.erase(old);
		}
		for (const auto& rig : delta.add) {
			entries.emplace(rig.bones[0], attach(rig));
		}
		for (auto& [node, entry] : entries) {
			if (entry.system && !entry.system->m_world) {
				world->addSkinnedMeshSystem(entry.system.get());
			}
		}
	}
}
