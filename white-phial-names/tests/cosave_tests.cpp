#include "CosaveFile.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace phial::storage;
namespace fs = std::filesystem;

template<class F> void rejects(F&& f) {
    bool caught = false; try { f(); } catch (const Error&) { caught = true; } assert(caught);
}
Bytes frame(const Bank& bank) {
    const auto payload = encode(bank);
    Writer chunk; chunk.u32(bankRecord); chunk.u32(1); chunk.u32(static_cast<std::uint32_t>(payload.size()));
    chunk.data.insert(chunk.data.end(), payload.begin(), payload.end());
    Writer w; w.u32(0x45534B53); w.u32(1); w.u32(0); w.u32(0); w.u32(1);
    w.u32(pluginID); w.u32(1); w.u32(static_cast<std::uint32_t>(chunk.data.size()));
    w.data.insert(w.data.end(), chunk.data.begin(), chunk.data.end()); return w.data;
}
void write(const fs::path& path, const Bytes& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    assert(out.good());
}
int main() {
    assert(cosaveFilename("Save30") == "Save30.skse");
    assert(cosaveFilename("Save30.ess") == "Save30.skse");
    assert(cosaveFilename("Save30.EsS") == "Save30.skse");
    assert(cosaveFilename("Quick save.ESS") == "Quick save.skse");
    assert(cosaveFilename("Save.ess_extra.ess") == "Save.ess_extra.skse");
    assert(cosaveFilename("Save\xE7\x93\xB6.ess") == "Save\xE7\x93\xB6.skse");
    assert(cosaveFilename(std::string_view("Save30.ess\0", 11)) == "Save30.skse");
    rejects([] { (void)cosaveFilename(""); });
    rejects([] { (void)cosaveFilename(".ess"); });
    rejects([] { (void)cosaveFilename(std::string_view("Save\0other.ess", 14)); });
    const auto directory = fs::temp_directory_path() /
        ("phial-cosave-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); } } cleanup{directory};
    const auto documents = directory / "Documents";
    const auto configured = fs::path("Profiles") / "Test Profile" / "Saves";
    const auto expected = documents / "My Games" / "Skyrim Special Edition" / configured / "Save30.skse";
    const auto selected = cosavePath(documents, configured, "Save30.ess");
    assert(selected == expected);
    assert(cosavePath(documents, "Saves", "Save30.ess") ==
        documents / "My Games" / "Skyrim Special Edition" / "Saves" / "Save30.skse");
#ifdef _WIN32
    // Exercise the actual Windows separators, drive and profile traversal.
    assert(cosavePath(L"C:\\Users\\Tester\\Documents", L"..\\MO2\\profiles\\Main\\saves\\", "Save30.ESS") ==
        fs::path(L"C:\\Users\\Tester\\Documents\\My Games\\Skyrim Special Edition\\..\\MO2\\profiles\\Main\\saves\\\\Save30.skse"));
    assert(cosavePath(L"D:\\Documents", L"\\ProfileSaves", "Save30") ==
        fs::path(L"D:\\Documents\\My Games\\Skyrim Special Edition\\\\ProfileSaves\\Save30.skse"));
    assert(cosavePath(L"D:\\Documents", L"Saves", "folder\\Save30.ess") ==
        fs::path(L"D:\\Documents\\My Games\\Skyrim Special Edition\\Saves\\folder\\Save30.skse"));
#endif
    Liquid liquid; liquid.name = "Protected test potion"; liquid.weight = 0.5f;
    liquid.effects = {{{"Skyrim.esm", 0x39E51}, 18, 0, 0, 120}};
    liquid.namedKeywords = {"OCF_VesselBottlePotion", "OCF_VesselBottle"};
    const Bank bank{{liquid}};
    const auto bytes = frame(bank);
    write(expected, bytes);
    // Reproduce the released bug: the only real file is Save30.skse, so the
    // old raw-name + extension lookup sees nothing for a .ess message.
    assert(!readCosaveFile(expected.parent_path() / "Save30.ess.skse"));
    for (const auto* name : {"Save30", "Save30.ess", "Save30.ESS"}) {
        const auto loaded = readCosaveFile(cosavePath(documents, configured, name));
        assert(loaded && *loaded == bytes);
        const auto record = bankFromCosave(*loaded);
        assert(record && *record == encode(bank)); // Normal SKSE callback must see identical bytes.
        assert(decode(*record) == bank);
        assert(fingerprint(decode(*record)) == fingerprint(bank));
    }
    // A same-named decoy in the default folder must never substitute for the
    // selected profile, and no neighbouring save may be used as a fallback.
    auto other = bank; other.liquids[0].name = "Other character";
    write(cosavePath(documents, "Saves", "Save30"), frame(other));
    assert(*bankFromCosave(*readCosaveFile(selected)) == encode(bank));
    assert(!readCosaveFile(cosavePath(documents, configured, "Missing.ess")));
    fs::remove(selected);
    assert(!readCosaveFile(selected));
    // A present empty/truncated/corrupt co-save is distinct from an absent
    // legacy co-save and must fail parsing, not silently become an empty bank.
    write(selected, {});
    auto empty = readCosaveFile(selected); assert(empty && empty->empty());
    rejects([&] { (void)bankFromCosave(*empty); });
    auto damaged = bytes; damaged.pop_back(); write(selected, damaged);
    rejects([&] { (void)bankFromCosave(*readCosaveFile(selected)); });
    fs::remove(selected); fs::create_directory(selected);
    rejects([&] { (void)readCosaveFile(selected); });
    std::cout << "Co-save lookup: .ess normalization, configured paths, exact bank reload, no fallback and invalid-file refusal passed\n";
}
