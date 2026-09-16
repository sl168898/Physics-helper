#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
#include "FlailRigScanner.h"
#include "hdtSkyrimSystem.h"

namespace hdt
{
	// Experimental, player/third-person-only manager. Built inside hdtSMP64;
	// it does not call an invented public FSMP plugin API or equip any armor.
	class FlailDirectSMP
	{
	public:
		static FlailDirectSMP& get();
		void configure();
		void beginLoad();
		void endLoad(bool succeeded = true);
		void update();
		void shutdown();
		bool hasActiveSystems() const;

	private:
		using Rig = flail::Rig<RE::NiNode>;
		struct Entry
		{
			Rig rig;
			// Hold the actual instance and its bones until the simulation is detached.
			std::vector<RE::NiPointer<RE::NiNode>> heldNodes;
			RE::BSTSmartPointer<SkyrimSystem> system;
		};

		bool enabled = false;
		bool loading = true;
		bool stopped = false;
		bool warnedLegacy = false;
		bool warnedLimit = false;
		std::unordered_map<RE::NiNode*, Entry> entries;

		static bool legacyDriverPresent(RE::NiNode* actorRoot);
		static void detach(Entry& entry);
		static Entry attach(const Rig& rig);
		void clear();
	};
}
