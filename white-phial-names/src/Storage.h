#pragma once
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
namespace phial::storage {
bool registerPapyrus(RE::BSScript::IVirtualMachine* vm);
void message(SKSE::MessagingInterface::Message* event);
void revert();
void save(SKSE::SerializationInterface* api);
void loadRecord(SKSE::SerializationInterface* api, std::uint32_t version, std::uint32_t length);
}
