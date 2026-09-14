#pragma once
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <vector>
namespace splat {
// Exact identity, not a content hash: source IDs are never reused in a Renderer.
// Stream cache files are immutable for the duration of their unique session path.
struct SourceSpan {
    uint64_t source;
    uint32_t offset, count;
    bool operator==(const SourceSpan &b) const { return source==b.source && offset==b.offset && count==b.count; }
};
using UploadRow = std::vector<SourceSpan>;
using UploadRows = std::vector<UploadRow>;
inline UploadRows MakeUploadRows(const std::vector<SourceSpan> &spans) {
    UploadRows rows(1);uint32_t used=0;
    for(auto span:spans)while(span.count){
        if(used==1024){rows.emplace_back();used=0;}
        const auto n=std::min(1024-used,span.count);
        auto &row=rows.back();
        if(!row.empty() && row.back().source==span.source && row.back().offset+row.back().count==span.offset)row.back().count+=n;
        else row.push_back({span.source,span.offset,n});
        used+=n;span.offset+=n;span.count-=n;
    }
    return rows;
}
inline bool SameUploadRow(const UploadRows &a,const UploadRows &b,size_t row) {
    return row<a.size() && row<b.size() && a[row]==b[row];
}
}
