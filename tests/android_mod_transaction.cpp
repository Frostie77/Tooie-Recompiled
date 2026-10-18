#include "../lib/RecompFrontend/recompui/src/composites/android_mod_transaction.h"
#include <fstream>
#include <iostream>
#include <memory>

using namespace recompui;
using Path = std::filesystem::path;
static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) throw std::runtime_error("transaction check " + std::to_string(__LINE__)); } while (0)
static void write(const Path& path, const std::string& value) { std::ofstream out(path, std::ios::binary); out << value; CHECK(bool(out)); }
static std::string read(const Path& path) { std::ifstream in(path, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }
template<class F> static void rejected(F&& function) { bool threw = false; try { function(); } catch (const std::exception&) { threw = true; } CHECK(threw); }

int main(int argc, char** argv) {
    CHECK(argc == 2);
    const Path root = argv[1];
    std::filesystem::create_directories(root);
    for (const std::string& name : {"../outside.nrm", "/absolute.nrm", "windows\\bad.nrm", "C:bad.nrm", ".hidden.nrm", "not-a-mod.zip"})
        CHECK(!AndroidModTransaction::package_name(name));
    CHECK(AndroidModTransaction::package_name("example.nrm"));
    CHECK(AndroidModTransaction::package_name("texture.rtz"));
    CHECK(!AndroidModTransaction::package_name("control\nname.nrm"));
    {
        AndroidModTransaction transaction(root);
        auto staged = transaction.reserve("new.nrm"); write(staged, "new");
        const Path scratch = transaction.staging_path();
        std::vector<std::string> errors;
        CHECK(transaction.commit(errors) && errors.empty());
        CHECK(read(root / "new.nrm") == "new" && !std::filesystem::exists(scratch));
        CHECK(transaction.commit(errors)); // Repeated delivery cannot move/delete an installed file.
        transaction.cancel(); CHECK(read(root / "new.nrm") == "new");
    }
    {
        auto transaction = std::make_shared<AndroidModTransaction>(root);
        write(transaction->reserve("new.nrm"), "replacement");
        const Path scratch = transaction->staging_path();
        auto prompt_copy = transaction;
        transaction.reset(); CHECK(std::filesystem::exists(scratch));
        prompt_copy.reset(); CHECK(!std::filesystem::exists(scratch) && read(root / "new.nrm") == "new");
    }
    {
        AndroidModTransaction transaction(root);
        write(transaction.reserve("new.nrm"), "cancelled");
        transaction.cancel(); CHECK(read(root / "new.nrm") == "new");
        rejected([&] { transaction.reserve("late.nrm"); });
    }
    {
        AndroidModTransaction transaction(root);
        write(transaction.reserve("duplicate.nrm"), "one");
        rejected([&] { transaction.reserve("duplicate.nrm"); });
        for (int i = 1; i < 32; ++i) transaction.reserve("mod-" + std::to_string(i) + ".nrm");
        rejected([&] { transaction.reserve("too-many.nrm"); });
    }
    std::filesystem::create_directory(root / "directory.nrm");
    std::filesystem::create_symlink(root / "new.nrm", root / "linked.nrm");
    {
        AndroidModTransaction transaction(root);
        rejected([&] { transaction.reserve("directory.nrm"); });
        rejected([&] { transaction.reserve("linked.nrm"); });
    }
    // Fail each backup/install rename in a real two-file replacement; roll back exact old bytes.
    for (int fail_at = 1; fail_at <= 4; ++fail_at) {
        write(root / "first.nrm", "old-first"); write(root / "second.rtz", "old-second");
        int calls = 0;
        AndroidModTransaction transaction(root, [&](const Path& from, const Path& to, std::error_code& ec) {
            if (++calls == fail_at) ec = std::make_error_code(std::errc::io_error);
            else std::filesystem::rename(from, to, ec);
        }, [&](const Path& from, const Path& to, std::error_code& ec) {
            if (++calls == fail_at) ec = std::make_error_code(std::errc::io_error);
            else std::filesystem::create_hard_link(from, to, ec);
        });
        write(transaction.reserve("first.nrm"), "new-first"); write(transaction.reserve("second.rtz"), "new-second");
        std::vector<std::string> errors;
        CHECK(!transaction.commit(errors) && !errors.empty());
        CHECK(read(root / "first.nrm") == "old-first" && read(root / "second.rtz") == "old-second");
        CHECK(!std::filesystem::exists(transaction.staging_path()));
    }
    {
        AndroidModTransaction transaction(root, [](const Path& from, const Path& to, std::error_code& ec) {
            std::filesystem::rename(from, to, ec);
        }, [](const Path&, const Path&, std::error_code& ec) { ec = std::make_error_code(std::errc::io_error); });
        write(transaction.reserve("brand-new.nrm"), "new-first");
        write(transaction.reserve("second.rtz"), "replacement");
        std::vector<std::string> errors;
        CHECK(!transaction.commit(errors) && !std::filesystem::exists(root / "brand-new.nrm"));
        CHECK(read(root / "second.rtz") == "old-second");
    }
    {
        AndroidModTransaction transaction(root);
        auto staged = transaction.reserve("new.nrm"); write(staged, "lost"); std::filesystem::remove(staged);
        std::vector<std::string> errors;
        CHECK(!transaction.commit(errors) && read(root / "new.nrm") == "new");
    }
    // If the filesystem also refuses restoration, retain the backup for recovery.
    Path retained;
    {
        int calls = 0;
        AndroidModTransaction transaction(root, [&](const Path& from, const Path& to, std::error_code& ec) {
            ++calls;
            if (calls == 4) ec = std::make_error_code(std::errc::io_error);
            else std::filesystem::rename(from, to, ec);
        }, [&](const Path& from, const Path& to, std::error_code& ec) {
            if (++calls == 3) ec = std::make_error_code(std::errc::io_error);
            else std::filesystem::create_hard_link(from, to, ec);
        });
        write(transaction.reserve("new.nrm"), "failed");
        write(transaction.reserve("second.rtz"), "second-failed");
        retained = transaction.staging_path();
        std::vector<std::string> errors;
        CHECK(!transaction.commit(errors) && errors.size() == 2);
        CHECK(read(retained / "backup-0") == "new" && read(root / "new.nrm") == "failed");
    }
    CHECK(read(retained / "backup-0") == "new");
    std::filesystem::rename(retained / "backup-0", root / "new.nrm");
    std::filesystem::remove_all(retained);
    std::cout << "PASS: " << checks << " transaction checks: stage/confirm/cancel, shared prompt lifetime, names/links/limits, every two-file rename failure and retained recovery backup\n";
}
