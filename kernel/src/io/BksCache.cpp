#include "brep/io/BksCache.h"

#include "brep/Log.h"

#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace brep::io
{
namespace
{

std::uint32_t Crc32Update(std::uint32_t crc, const std::uint8_t* data,
                          std::size_t len)
{
    crc = ~crc;
    for (std::size_t i = 0; i < len; ++i)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b)
        {
            const std::uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

class BinWriter
{
public:
    void U8(std::uint8_t v)
    {
        m_buf.push_back(v);
    }

    void U32(std::uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
        {
            m_buf.push_back(std::uint8_t((v >> (8 * i)) & 0xff));
        }
    }

    void F64(double v)
    {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &v, 8);
        for (int i = 0; i < 8; ++i)
        {
            m_buf.push_back(std::uint8_t((bits >> (8 * i)) & 0xff));
        }
    }

    void WriteGuid(const Guid& g)
    {
        for (std::uint8_t b : g.Bytes())
        {
            m_buf.push_back(b);
        }
    }

    void Bytes(const std::uint8_t* p, std::size_t n)
    {
        m_buf.insert(m_buf.end(), p, p + n);
    }

    [[nodiscard]] const std::vector<std::uint8_t>& Data() const
    {
        return m_buf;
    }

private:
    std::vector<std::uint8_t> m_buf;
};

class BinReader
{
public:
    explicit BinReader(std::vector<std::uint8_t> data)
        : m_buf(std::move(data))
    {
    }

    [[nodiscard]] bool Ok() const noexcept
    {
        return m_ok;
    }

    [[nodiscard]] const std::string& Error() const
    {
        return m_error;
    }

    std::uint8_t U8()
    {
        if (m_pos >= m_buf.size())
        {
            Fail("trunc");
            return 0;
        }
        return m_buf[m_pos++];
    }

    std::uint32_t U32()
    {
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
        {
            v |= std::uint32_t(U8()) << (8 * i);
        }
        return v;
    }

    double F64()
    {
        std::uint64_t bits = 0;
        for (int i = 0; i < 8; ++i)
        {
            bits |= std::uint64_t(U8()) << (8 * i);
        }
        double v = 0;
        std::memcpy(&v, &bits, 8);
        return v;
    }

    Guid ReadGuid()
    {
        std::array<std::uint8_t, 16> b{};
        for (int i = 0; i < 16; ++i)
        {
            b[static_cast<std::size_t>(i)] = U8();
        }
        return Guid::FromBytes(b);
    }

    [[nodiscard]] bool Finished() const noexcept
    {
        return m_pos == m_buf.size();
    }

private:
    void Fail(const char* message)
    {
        m_ok = false;
        if (m_error.empty())
        {
            m_error = message;
        }
    }

    std::vector<std::uint8_t> m_buf;
    std::size_t m_pos{0};
    bool m_ok{true};
    std::string m_error;
};

void WriteMesh(BinWriter& w, const TriangleMesh& tri, const EdgeMesh& edges)
{
    w.U32(static_cast<std::uint32_t>(tri.Vertices.size()));
    for (const auto& v : tri.Vertices)
    {
        w.F64(v.Position.x());
        w.F64(v.Position.y());
        w.F64(v.Position.z());
        w.F64(v.Normal.x());
        w.F64(v.Normal.y());
        w.F64(v.Normal.z());
        w.F64(v.Uv.u());
        w.F64(v.Uv.v());
    }
    w.U32(static_cast<std::uint32_t>(tri.Indices.size()));
    for (std::uint32_t idx : tri.Indices)
    {
        w.U32(idx);
    }

    w.U32(static_cast<std::uint32_t>(edges.Positions.size()));
    for (const auto& p : edges.Positions)
    {
        w.F64(p.x());
        w.F64(p.y());
        w.F64(p.z());
    }
}

bool ReadMesh(BinReader& r, TriangleMesh& tri, EdgeMesh& edges)
{
    const std::uint32_t vertexCount = r.U32();
    tri.Vertices.resize(vertexCount);
    for (std::uint32_t i = 0; i < vertexCount; ++i)
    {
        tri.Vertices[i].Position = Point3d{r.F64(), r.F64(), r.F64()};
        tri.Vertices[i].Normal = Vector3d{r.F64(), r.F64(), r.F64()};
        tri.Vertices[i].Uv = Point2d{r.F64(), r.F64()};
    }
    const std::uint32_t indexCount = r.U32();
    tri.Indices.resize(indexCount);
    for (std::uint32_t i = 0; i < indexCount; ++i)
    {
        tri.Indices[i] = r.U32();
    }

    const std::uint32_t edgePointCount = r.U32();
    edges.Positions.resize(edgePointCount);
    for (std::uint32_t i = 0; i < edgePointCount; ++i)
    {
        edges.Positions[i] = Point3d{r.F64(), r.F64(), r.F64()};
    }
    return r.Ok();
}

}  // namespace

std::filesystem::path BksCachePathFor(const std::filesystem::path& xlPath)
{
    return xlPath.parent_path() /
           (xlPath.stem().string() + std::string(".bks.cache"));
}

CacheSaveResult SaveBksCache(const Document& doc,
                             const std::filesystem::path& xlPath)
{
    CacheSaveResult result;
    try
    {
        BinWriter w;
        w.Bytes(reinterpret_cast<const std::uint8_t*>("BKSC"), 4);
        w.U32(1);  // cache schema
        w.WriteGuid(doc.Guid);

        std::uint32_t bodyCount = 0;
        for (const auto& part : doc.Parts())
        {
            bodyCount += static_cast<std::uint32_t>(part->Model().Bodies().size());
        }
        w.U32(bodyCount);

        for (const auto& part : doc.Parts())
        {
            for (const auto& body : part->Model().Bodies())
            {
                if (!body)
                {
                    continue;
                }
                w.WriteGuid(body->Guid);
                WriteMesh(w, TessellateBody(*body), ExtractEdges(*body));
            }
        }

        const std::uint32_t sum =
            Crc32Update(0, w.Data().data(), w.Data().size());
        w.U32(sum);

        const auto cachePath = BksCachePathFor(xlPath);
        std::ofstream out(cachePath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            result.Error = "cannot write cache";
            return result;
        }
        out.write(reinterpret_cast<const char*>(w.Data().data()),
                  static_cast<std::streamsize>(w.Data().size()));
        result.Ok = static_cast<bool>(out);
        if (result.Ok)
        {
            BREP_INFO("saved mesh cache '{}' bodies={}", cachePath.string(),
                      bodyCount);
        }
        return result;
    }
    catch (const std::exception& ex)
    {
        result.Error = ex.what();
        return result;
    }
}

CacheLoadResult LoadBksCache(const std::filesystem::path& xlPath,
                             const Guid& expectedDocumentGuid)
{
    CacheLoadResult result;
    try
    {
        const auto cachePath = BksCachePathFor(xlPath);
        std::ifstream in(cachePath, std::ios::binary);
        if (!in)
        {
            result.Error = "no cache";
            return result;
        }
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        if (bytes.size() < 4 + 4 + 16 + 4 + 4)
        {
            result.Error = "cache too small";
            return result;
        }

        // Split CRC (last 4 LE bytes)
        std::uint32_t fileCrc = 0;
        for (int i = 0; i < 4; ++i)
        {
            fileCrc |= std::uint32_t(
                           bytes[bytes.size() - 4 + static_cast<std::size_t>(i)])
                       << (8 * i);
        }
        bytes.resize(bytes.size() - 4);
        const std::uint32_t calc = Crc32Update(0, bytes.data(), bytes.size());
        if (calc != fileCrc)
        {
            result.Error = "cache CRC mismatch";
            return result;
        }

        BinReader r(std::move(bytes));
        if (r.U8() != 'B' || r.U8() != 'K' || r.U8() != 'S' || r.U8() != 'C')
        {
            result.Error = "bad cache magic";
            return result;
        }
        const std::uint32_t schema = r.U32();
        if (schema != 1)
        {
            result.Error = "unsupported cache schema";
            return result;
        }
        result.Cache.DocumentGuid = r.ReadGuid();
        if (result.Cache.DocumentGuid != expectedDocumentGuid)
        {
            result.Error = "cache document Guid mismatch";
            return result;
        }

        const std::uint32_t bodyCount = r.U32();
        for (std::uint32_t i = 0; i < bodyCount; ++i)
        {
            const Guid bodyGuid = r.ReadGuid();
            TriangleMesh tri;
            EdgeMesh edges;
            if (!ReadMesh(r, tri, edges))
            {
                result.Error = r.Error();
                return result;
            }
            result.Cache.Triangles.emplace(bodyGuid, std::move(tri));
            result.Cache.Edges.emplace(bodyGuid, std::move(edges));
        }
        if (!r.Ok() || !r.Finished())
        {
            result.Error = "cache decode failed";
            return result;
        }
        result.Ok = true;
        BREP_INFO("loaded mesh cache bodies={}", bodyCount);
        return result;
    }
    catch (const std::exception& ex)
    {
        result.Error = ex.what();
        return result;
    }
}

}  // namespace brep::io
