#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
// The portable part of the experimental GildFlail direct-physics extension.
#include <array>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace hdt::flail
{
	inline constexpr std::array<std::string_view, 5> boneNames{
		"Flail_0", "Flail_1", "Flail_2", "Flail_3", "Flail_4"
	};

	template <class Node>
	struct Rig
	{
		std::vector<Node*> bones;
		// Includes the actor's root, all attachment ancestors, and Flail_0.
		// Reparenting an unchanged weapon is therefore an identity change too.
		std::vector<Node*> attachment;
		bool operator==(const Rig&) const = default;
	};

	template <class Node>
	struct Scan
	{
		std::vector<Rig<Node>> rigs;
		bool complete = true;
		std::size_t visited = 0;
	};

	// Traits supplies name(Node*), hidden(Node*), and children(Node*, callback).
	// Accepts the supplied contiguous 2-, 4-, and 5-bone GildFlail rigs.
	// There is no fallback search in another weapon or the actor's skeleton.
	template <class Node, class Traits>
	Scan<Node> collect(Node* root, std::size_t maxNodes = 20000, std::size_t maxDepth = 96)
	{
		Scan<Node> result;
		std::vector<Node*> path;
		auto visit = [&](auto&& self, Node* node) -> void {
			if (!node || !result.complete) {
				return;
			}
			if (++result.visited > maxNodes || path.size() >= maxDepth) {
				result.complete = false;
				return;
			}
			if (Traits::hidden(node)) {
				return;
			}
			path.push_back(node);
			if (Traits::name(node) == boneNames[0]) {
				Rig<Node> rig;
				rig.bones.push_back(node);
				bool valid = true;
				for (std::size_t i = 1; i <= boneNames.size(); ++i) {
					Node* next = nullptr;
					std::size_t matches = 0;
					Traits::children(rig.bones.back(), [&](Node* child) {
						if (!child || !Traits::name(child).starts_with("Flail_")) return;
						// Reject gaps, duplicates, branches and chains longer than known rigs.
						if (i >= boneNames.size() || Traits::name(child) != boneNames[i]) {
							valid = false;
						} else {
							++matches;
							next = child;
						}
					});
					if (!valid || matches > 1 || (next && Traits::hidden(next))) {
						valid = false;
						break;
					}
					if (!next) break;
					rig.bones.push_back(next);
				}
				const auto count = rig.bones.size();
				valid = valid && (count == 2 || count == 4 || count == 5);
				if (valid) {
					rig.attachment = path;
					result.rigs.push_back(std::move(rig));
				}
			} else {
				Traits::children(node, [&](Node* child) { self(self, child); });
			}
			path.pop_back();
		};
		visit(visit, root);
		if (!result.complete) {
			// A partial traversal must never leave an arbitrarily chosen subset live.
			result.rigs.clear();
		}
		return result;
	}

	template <class Node>
	struct Changes
	{
		std::vector<Node*> remove;
		std::vector<Rig<Node>> add;
	};

	template <class Node>
	Changes<Node> changes(const std::vector<Rig<Node>>& previous, const std::vector<Rig<Node>>& next)
	{
		Changes<Node> result;
		for (const auto& old : previous) {
			bool unchanged = false;
			for (const auto& now : next) {
				if (old == now) {
					unchanged = true;
					break;
				}
			}
			if (!unchanged) {
				result.remove.push_back(old.bones[0]);
			}
		}
		for (const auto& now : next) {
			bool unchanged = false;
			for (const auto& old : previous) {
				if (old == now) {
					unchanged = true;
					break;
				}
			}
			if (!unchanged) {
				result.add.push_back(now);
			}
		}
		return result;
	}
}
