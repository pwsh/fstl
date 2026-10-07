// Loader robustness tests (non-GUI): malformed and hostile .stl files must
// be rejected cleanly, and minimal valid files must load.
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#include "../src/loader.h"

namespace
{
enum class Result { Mesh, BadStl, Empty, Missing, Nothing };

struct Outcome {
    Result result = Result::Nothing;
    size_t vertices = 0;
    int triangles = 0;
};

Outcome load(const QString& path)
{
    Outcome out;
    Loader loader(nullptr, path, false);
    QObject::connect(
        &loader, &Loader::got_mesh, &loader,
        [&](Mesh* m, bool) {
            out.result = Result::Mesh;
            out.vertices = m->vertexCount();
            out.triangles = m->triCount();
            delete m;
        },
        Qt::DirectConnection);
    QObject::connect(
        &loader, &Loader::error_bad_stl, &loader,
        [&] {
            out.result = Result::BadStl;
        },
        Qt::DirectConnection);
    QObject::connect(
        &loader, &Loader::error_empty_mesh, &loader,
        [&] {
            out.result = Result::Empty;
        },
        Qt::DirectConnection);
    QObject::connect(
        &loader, &Loader::error_missing_file, &loader,
        [&] {
            out.result = Result::Missing;
        },
        Qt::DirectConnection);
    loader.run(); // synchronous
    return out;
}

void put_u32(QByteArray& b, uint32_t v)
{
    char buf[4];
    qToLittleEndian<quint32>(v, buf);
    b.append(buf, 4);
}

void put_f32(QByteArray& b, float f)
{
    char buf[4];
    qToLittleEndian<float>(f, buf);
    b.append(buf, 4);
}

/*  A binary STL with the given triangles (each 9 floats) and declared count */
QByteArray binary_stl(const std::vector<float>& coords, uint32_t declared)
{
    QByteArray b(80, ' ');
    put_u32(b, declared);
    for (size_t t = 0; t + 9 <= coords.size(); t += 9) {
        for (int i = 0; i < 3; ++i) {
            put_f32(b, 0); // normal
        }
        for (int i = 0; i < 9; ++i) {
            put_f32(b, coords[t + i]);
        }
        b.append(2, '\0'); // attribute
    }
    return b;
}

QByteArray ascii_stl(const char* v3)
{
    return QByteArray("solid test\n"
                      "  facet normal 0 0 1\n"
                      "    outer loop\n"
                      "      vertex 0 0 0\n"
                      "      vertex 1 0 0\n"
                      "      vertex ") +
           v3 +
           "\n"
           "    endloop\n"
           "  endfacet\n"
           "endsolid test\n";
}

bool write_file(const QString& path, const QByteArray& data)
{
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

int failures = 0;

void check(bool cond, const char* what)
{
    printf("%s: %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) {
        ++failures;
    }
}

/*  Header declaring `count` triangles, padded (sparsely) to the size that
 *  count implies.  Returns false if the filesystem can't hold it. */
bool sparse_stl(const QString& path, uint32_t count)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    QByteArray header(80, ' ');
    put_u32(header, count);
    f.write(header);
    // Fill the first record with real data so a buggy reader starts parsing
    f.write(QByteArray(50, '\0'));
    return f.resize(84 + qint64(count) * 50);
}
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid()) {
        printf("FAIL: no temporary directory\n");
        return 1;
    }
    const std::vector<float> tri = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    // Valid one-triangle binary and ASCII files
    const QString bin = dir.filePath("valid_bin.stl");
    write_file(bin, binary_stl(tri, 1));
    Outcome o = load(bin);
    check(o.result == Result::Mesh && o.vertices == 3 && o.triangles == 1, "valid binary loads with 3 vertices");

    const QString asc = dir.filePath("valid_ascii.stl");
    write_file(asc, ascii_stl("0 1 0"));
    o = load(asc);
    check(o.result == Result::Mesh && o.vertices == 3 && o.triangles == 1, "valid ASCII loads with 3 vertices");

    // Truncated binary: declares 2 triangles but holds 1
    const QString trunc = dir.filePath("truncated.stl");
    write_file(trunc, binary_stl(tri, 2));
    check(load(trunc).result == Result::BadStl, "truncated binary rejected");

    // Header only, shorter than 84 bytes
    const QString tiny = dir.filePath("tiny.stl");
    write_file(tiny, QByteArray(40, 'x'));
    check(load(tiny).result == Result::BadStl, "short file rejected");

    // Non-finite coordinates
    for (const float bad : {nan, inf, -inf}) {
        std::vector<float> t = tri;
        t[4] = bad;
        const QString p = dir.filePath("nonfinite_bin.stl");
        write_file(p, binary_stl(t, 1));
        check(load(p).result == Result::BadStl, "non-finite binary vertex rejected");
    }
    for (const char* bad : {"nan 0 0", "0 inf 0", "0 0 -inf"}) {
        const QString p = dir.filePath("nonfinite_ascii.stl");
        write_file(p, ascii_stl(bad));
        check(load(p).result == Result::BadStl, "non-finite ASCII vertex rejected");
    }

    // Huge declared counts with a matching (sparse) file size: tri_count * 3
    // overflowed uint32 and undersized the vertex buffer
    for (const uint32_t count : {0xFFFFFFFFu, 1431655766u}) {
        const QString p = dir.filePath("huge.stl");
        if (!sparse_stl(p, count)) {
            printf("SKIP: could not create a sparse file for tri_count=%u\n", count);
            QFile::remove(p);
            continue;
        }
        char what[96];
        snprintf(what, sizeof(what), "huge tri_count=%u rejected", count);
        check(load(p).result == Result::BadStl, what);
        QFile::remove(p);
    }

    // Missing file
    check(load(dir.filePath("does_not_exist.stl")).result == Result::Missing, "missing file reported");

    printf(failures ? "FAILED (%d)\n" : "ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
