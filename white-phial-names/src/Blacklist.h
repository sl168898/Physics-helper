#pragma once
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
namespace phial::blacklist {
bool registerPapyrus(RE::BSScript::IVirtualMachine* vm);
void message(SKSE::MessagingInterface::Message* event);
}
