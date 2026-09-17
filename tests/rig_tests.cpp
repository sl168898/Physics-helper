#include "FlailRigScanner.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>

struct Node
{
	std::string name;
	bool hidden = false;
	std::vector<Node*> children;
};

struct Traits
{
	static std::string_view name(Node* n) { return n->name; }
	static bool hidden(Node* n) { return n->hidden; }
	template <class F> static void children(Node* n, F&& f)
	{
		for (auto child : n->children) f(child);
	}
};

struct Scene
{
	std::vector<std::unique_ptr<Node>> nodes;
	Node* make(std::string name)
	{
		nodes.push_back(std::make_unique<Node>(Node{std::move(name), false, {}}));
		return nodes.back().get();
	}
	Node* rig(Node* parent, int count = 5)
	{
		auto root = make("Flail_0");
		parent->children.push_back(root);
		auto bone = root;
		for (int i = 1; i < count; ++i) {
			auto child = make("Flail_" + std::to_string(i));
			bone->children.push_back(child);
			bone = child;
		}
		return root;
	}
};

int main()
{
	using namespace hdt::flail;
	Scene s;
	auto actor = s.make("NPC");
	auto left = s.make("Left hand");
	auto right = s.make("Right hand");
	auto display = s.make("IED display");
	actor->children = {left, right, display};
	auto l = s.rig(left);
	auto r = s.rig(right);
	auto d = s.rig(display);
	display->hidden = true;
	auto read = [&] { return collect<Node, Traits>(actor); };

	// Two weapons with identical names still produce disjoint node bindings.
	auto initial = read();
	assert(initial.complete && initial.rigs.size() == 2);
	for (std::size_t i = 0; i < 5; ++i) assert(initial.rigs[0].bones[i] != initial.rigs[1].bones[i]);
	auto delta = changes(std::vector<Rig<Node>>{}, initial.rigs);
	assert(delta.add.size() == 2 && delta.remove.empty());

	// Stable and reordered scenes must not recreate physics every frame.
	delta = changes(initial.rigs, read().rigs);
	assert(delta.add.empty() && delta.remove.empty());
	actor->children = {right, display, left};
	delta = changes(initial.rigs, read().rigs);
	assert(delta.add.empty() && delta.remove.empty());

	// Unequip/display swap: the live right-hand flail is not touched.
	left->hidden = true;
	display->hidden = false;
	auto swapped = read();
	delta = changes(initial.rigs, swapped.rigs);
	assert(delta.remove == std::vector<Node*>{l});
	assert(delta.add.size() == 1 && delta.add[0].bones[0] == d);

	// A retained instance moves from a hand to a sheath without pointer changes.
	auto sheath = s.make("Sheath");
	actor->children.push_back(sheath);
	right->children.clear();
	sheath->children.push_back(r);
	auto moved = read();
	delta = changes(swapped.rigs, moved.rigs);
	assert(delta.remove == std::vector<Node*>{r});
	assert(delta.add.size() == 1 && delta.add[0].bones[0] == r);

	// Replacing bones inside a retained root must invalidate the old binding.
	auto bone2 = r->children[0]->children[0];
	auto replacement3 = s.make("Flail_3");
	replacement3->children.push_back(s.make("Flail_4"));
	bone2->children = {replacement3};
	auto replaced = read();
	delta = changes(moved.rigs, replaced.rigs);
	assert(delta.remove == std::vector<Node*>{r});
	assert(delta.add.size() == 1 && delta.add[0].bones[3] == replacement3);

	// Broken rigs cannot borrow identically named bones from the other flail.
	bone2->children.clear();
	auto broken = read();
	assert(broken.rigs.size() == 1 && broken.rigs[0].bones[0] == d);
	delta = changes(replaced.rigs, broken.rigs);
	assert(delta.remove == std::vector<Node*>{r} && delta.add.empty());

	// Ambiguous duplicates inside one chain are rejected.
	d->children.push_back(s.make("Flail_1"));
	assert(read().rigs.empty());
	d->children.pop_back();

	// Hiding an intermediate bone rejects that rig too.
	d->children[0]->hidden = true;
	assert(read().rigs.empty());
	d->children[0]->hidden = false;

	// Unload removes every binding without touching a new scene's equal names.
	delta = changes(initial.rigs, collect<Node, Traits>(nullptr).rigs);
	assert(delta.remove.size() == 2 && delta.add.empty());
	Scene newer;
	auto newActor = newer.make("NPC");
	auto newFlail = newer.rig(newActor);
	delta = changes(initial.rigs, collect<Node, Traits>(newActor).rigs);
	assert(delta.remove.size() == 2 && delta.add.size() == 1 && delta.add[0].bones[0] == newFlail);

	// Traversal overflow is all-or-nothing, never a partial one-flail result.
	auto limited = collect<Node, Traits>(actor, 2);
	assert(!limited.complete && limited.rigs.empty());
	limited = collect<Node, Traits>(actor, 20000, 1);
	assert(!limited.complete && limited.rigs.empty());
	auto cycle = s.make("cycle");
	cycle->children.push_back(cycle);
	limited = collect<Node, Traits>(cycle, 100, 20);
	assert(!limited.complete && limited.rigs.empty());

	// Actual two-handed mesh topologies: Iron has 2 bones, Steel has 4.
	Scene mixed;
	auto mixedActor = mixed.make("NPC");
	auto iron = mixed.rig(mixedActor, 2);
	auto steel = mixed.rig(mixedActor, 4);
	mixed.rig(mixedActor, 5);
	auto all = collect<Node, Traits>(mixedActor);
	assert(all.complete && all.rigs.size() == 3);
	assert(all.rigs[0].bones.size() == 2 && all.rigs[1].bones.size() == 4 && all.rigs[2].bones.size() == 5);
	assert(all.rigs[0].bones.back() == iron->children[0]);
	assert(changes(all.rigs, collect<Node, Traits>(mixedActor).rigs).add.empty());
	// A gap must not be mistaken for a legitimate short chain.
	iron->children[0]->children.push_back(mixed.make("Flail_3"));
	assert((collect<Node, Traits>(mixedActor).rigs.size() == 2));
	iron->children[0]->children.clear();
	// Hidden short chains are rejected, not shortened further.
	steel->children[0]->children[0]->hidden = true;
	assert((collect<Node, Traits>(mixedActor).rigs.size() == 2));
	// A short chain may grow when the game replaces its model; rebind once.
	iron->children[0]->children.push_back(mixed.make("Flail_2"));
	assert((collect<Node, Traits>(mixedActor).rigs.size() == 1));
	// No unsupported single-bone or six-bone rigs.
	Scene unsupported;
	auto other = unsupported.make("NPC");
	unsupported.rig(other, 1);
	unsupported.rig(other, 6);
	assert((collect<Node, Traits>(other).rigs.empty()));

	std::cout << "PASS: duplicate rigs, stability, visibility swap, reparenting, bone replacement,\n"
	             "2/4/5-bone rigs, gaps/ambiguous/hidden bones, unload, new scene, node/depth/cycle limits\n";
}
