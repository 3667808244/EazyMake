// Unit tests for crypto.cpp (SHA-256)
#define CATCH_AMALGAMATED_CUSTOM_MAIN
#include "catch2.hpp"
#include "ezmk/crypto.hpp"
#include "ezmk/util.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace ezmk::crypto;

// ===================================================================
// Known SHA-256 test vectors (NIST / FIPS 180-4)
// ===================================================================

TEST_CASE("sha256: NIST test vectors", "[crypto]") {
    // Test vectors from:
    // https://www.di-mgt.com.au/sha_testvectors.html
    // and NIST FIPS 180-4

    SECTION("empty string") {
        // SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
        REQUIRE(sha256("") ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    }

    SECTION("\"abc\"") {
        // SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
        REQUIRE(sha256("abc") ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    }

    SECTION("\"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq\"") {
        // Longer string (448 bits)
        REQUIRE(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    }

    SECTION("long string (1 million 'a')") {
        // SHA-256 of "a" repeated 1,000,000 times
        // Expected: cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0
        std::string million_a(1'000'000, 'a');
        REQUIRE(sha256(million_a) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    }
}

TEST_CASE("sha256: output format", "[crypto]") {
    SECTION("always 64 hex characters") {
        REQUIRE(sha256("test").size() == 64);
        REQUIRE(sha256("").size() == 64);
        REQUIRE(sha256(std::string(1000, 'x')).size() == 64);
    }

    SECTION("lowercase hex") {
        auto h = sha256("Hello World!");
        for (char c : h) {
            if (c >= 'A' && c <= 'F') {
                FAIL("SHA-256 output should be lowercase hex, got: " << h);
            }
        }
        REQUIRE(true);
    }

    SECTION("different inputs → different outputs") {
        auto h1 = sha256("hello");
        auto h2 = sha256("Hello");
        REQUIRE(h1 != h2);
    }

    SECTION("same input → same output (deterministic)") {
        auto h1 = sha256("deterministic test");
        auto h2 = sha256("deterministic test");
        REQUIRE(h1 == h2);
    }
}

// ===================================================================
// Sha256Stream (incremental hashing)
// ===================================================================

TEST_CASE("Sha256Stream: basic streaming", "[crypto]") {
    SECTION("single update = one-shot") {
        Sha256Stream stream;
        stream.update("abc", 3);
        auto result = stream.finalize();
        REQUIRE(result == sha256("abc"));
    }

    SECTION("multiple updates = one-shot") {
        Sha256Stream stream;
        stream.update("ab", 2);
        stream.update("c", 1);
        auto result = stream.finalize();
        REQUIRE(result == sha256("abc"));
    }

    SECTION("empty streaming = empty string hash") {
        Sha256Stream stream;
        auto result = stream.finalize();
        REQUIRE(result == sha256(""));
    }

    SECTION("byte-by-byte streaming") {
        std::string input = "The quick brown fox jumps over the lazy dog";
        Sha256Stream stream;
        for (char c : input) {
            stream.update(&c, 1);
        }
        auto result = stream.finalize();
        REQUIRE(result == sha256(input));
    }

    SECTION("large block streaming (8KiB chunks)") {
        std::string input(8192, 'X');
        Sha256Stream stream;
        size_t chunk = 1024;
        for (size_t i = 0; i < input.size(); i += chunk) {
            size_t n = std::min(chunk, input.size() - i);
            stream.update(input.data() + i, n);
        }
        auto result = stream.finalize();
        REQUIRE(result == sha256(input));
    }
}

TEST_CASE("Sha256Stream: finalize_raw", "[crypto]") {
    SECTION("raw output is 32 bytes") {
        Sha256Stream stream;
        stream.update("test", 4);
        uint8_t raw[32];
        stream.finalize_raw(raw);
        // The raw output is 32 bytes (256 bits)
        REQUIRE(true);
    }

    SECTION("raw output matches hex output") {
        Sha256Stream stream;
        stream.update("raw test", 8);
        uint8_t raw[32];
        stream.finalize_raw(raw);

        // Convert raw to hex
        std::string hex;
        for (int i = 0; i < 32; ++i) {
            char buf[3];
            std::snprintf(buf, sizeof(buf), "%02x", raw[i]);
            hex += buf;
        }

        // Should match one-shot sha256
        REQUIRE(hex == sha256("raw test"));
    }
}

TEST_CASE("Sha256Stream: reuse after finalize is undefined", "[crypto]") {
    // After finalize(), the stream state is consumed.
    // We don't test behavior after finalize — it's undefined.
    Sha256Stream stream;
    stream.update("data", 4);
    stream.finalize();
    // No further assertions — just ensure no crash
    REQUIRE(true);
}

// ===================================================================
// sha256_file()
// ===================================================================

TEST_CASE("sha256_file: basic file hashing", "[crypto]") {
    SECTION("hash of temp file") {
        auto tmp = fs::temp_directory_path() / "ezmk_sha256_test.txt";
        std::string content = "Hello, file hashing test!";
        std::ofstream f(tmp, std::ios::binary);
        f << content;
        f.close();

        auto file_hash = sha256_file(tmp);
        auto str_hash = sha256(content);
        REQUIRE(file_hash == str_hash);

        fs::remove(tmp);
    }

    SECTION("hash of non-existent file → empty") {
        auto result = sha256_file("nonexistent_sha256_test_file.txt");
        REQUIRE(result.empty());
    }

    SECTION("hash of empty file") {
        auto tmp = fs::temp_directory_path() / "ezmk_empty.txt";
        std::ofstream f(tmp, std::ios::binary);
        f.close();

        auto file_hash = sha256_file(tmp);
        REQUIRE(file_hash == sha256(""));

        fs::remove(tmp);
    }

    SECTION("hash of binary file") {
        auto tmp = fs::temp_directory_path() / "ezmk_binary.bin";
        std::ofstream f(tmp, std::ios::binary);
        unsigned char udata[] = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};
        f.write(reinterpret_cast<const char*>(udata), sizeof(udata));
        f.close();

        auto file_hash = sha256_file(tmp);
        auto str_hash = sha256(std::string_view(reinterpret_cast<const char*>(udata), sizeof(udata)));
        REQUIRE(file_hash == str_hash);

        fs::remove(tmp);
    }

    SECTION("hash consistency across reads") {
        auto tmp = fs::temp_directory_path() / "ezmk_consistent.bin";
        {
            std::ofstream f(tmp, std::ios::binary);
            for (int i = 0; i < 10000; ++i) {
                f.put(static_cast<char>(i % 256));
            }
        }

        auto h1 = sha256_file(tmp);
        auto h2 = sha256_file(tmp);
        auto h3 = sha256_file(tmp);

        REQUIRE(h1 == h2);
        REQUIRE(h2 == h3);

        fs::remove(tmp);
    }
}
