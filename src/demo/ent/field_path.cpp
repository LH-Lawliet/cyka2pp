// Huffman decode of field-path operations, ported from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/{huffman,field_path}.go. See NOTICE.

#include "cyka/demo/ent/field_path.hpp"

#include "cyka/demo/ent/field_path_ops.hpp"

#include <memory>
#include <queue>
#include <vector>

namespace cyka::demo::ent {
namespace {

/// Flat huffman node: `left < 0` marks a leaf whose `value` is an op index.
struct HuffNode {
    std::int16_t left{0};
    std::int16_t right{0};
    std::int16_t value{0};
};

struct BuildNode {
    int weight{0};
    int value{0};
    std::unique_ptr<BuildNode> left;
    std::unique_ptr<BuildNode> right;
    [[nodiscard]] bool leaf() const noexcept { return left == nullptr; }
};

/// demoinfocs' treeHeap ordering: lowest weight first, higher op value wins ties.
struct WorseThan {
    bool operator()(const BuildNode* a, const BuildNode* b) const noexcept {
        if (a->weight != b->weight) {
            return a->weight > b->weight;
        }
        return a->value < b->value;
    }
};

std::int16_t flatten(const BuildNode& node, std::vector<HuffNode>& out) {
    const auto idx = static_cast<std::int16_t>(out.size());
    out.emplace_back();
    if (node.leaf()) {
        out[static_cast<std::size_t>(idx)].left = -1;
        out[static_cast<std::size_t>(idx)].value = static_cast<std::int16_t>(node.value);
        return idx;
    }
    const std::int16_t l = flatten(*node.left, out);
    const std::int16_t r = flatten(*node.right, out);
    out[static_cast<std::size_t>(idx)].left = l;
    out[static_cast<std::size_t>(idx)].right = r;
    return idx;
}

const std::vector<HuffNode>& huff_nodes() {
    static const std::vector<HuffNode> nodes = [] {
        std::vector<std::unique_ptr<BuildNode>> owned;
        std::priority_queue<BuildNode*, std::vector<BuildNode*>, WorseThan> heap;
        for (std::size_t i = 0; i < kFieldPathOps.size(); ++i) {
            auto leaf = std::make_unique<BuildNode>();
            leaf->weight = kFieldPathOps[i].weight == 0 ? 1 : kFieldPathOps[i].weight;
            leaf->value = static_cast<int>(i);
            heap.push(leaf.get());
            owned.push_back(std::move(leaf));
        }
        int next_value = static_cast<int>(kFieldPathOps.size());
        while (heap.size() > 1) {
            BuildNode* a = heap.top();
            heap.pop();
            BuildNode* b = heap.top();
            heap.pop();
            auto parent = std::make_unique<BuildNode>();
            parent->weight = a->weight + b->weight;
            parent->value = next_value++;
            // Ownership transfer: `owned` keeps the arena alive until flatten().
            for (auto& p : owned) {
                if (p.get() == a) {
                    parent->left = std::move(p);
                } else if (p.get() == b) {
                    parent->right = std::move(p);
                }
            }
            heap.push(parent.get());
            owned.push_back(std::move(parent));
        }
        std::vector<HuffNode> flat;
        flat.reserve(2 * kFieldPathOps.size());
        if (!owned.empty()) {
            flatten(*owned.back(), flat);
        }
        return flat;
    }();
    return nodes;
}

} // namespace

int read_field_paths(BitStream& r, std::vector<FieldPath>& out) {
    const auto& nodes = huff_nodes();
    if (nodes.empty()) {
        return 0;
    }
    FieldPath fp;
    int count = 0;
    std::int16_t node = 0;
    // Source 2 packs at most a few hundred field updates per entity; the cap
    // only guards against a desynced stream looping forever.
    constexpr int kMaxUpdates = 4096;
    while (!fp.done && !r.failed() && count < kMaxUpdates) {
        const std::int16_t next = r.read_bool() ? nodes[static_cast<std::size_t>(node)].right
                                                : nodes[static_cast<std::size_t>(node)].left;
        if (next < 0 || static_cast<std::size_t>(next) >= nodes.size()) {
            return -1;
        }
        if (nodes[static_cast<std::size_t>(next)].left < 0) {
            node = 0;
            const auto op = static_cast<std::size_t>(nodes[static_cast<std::size_t>(next)].value);
            if (op >= kFieldPathOps.size()) {
                return -1;
            }
            kFieldPathOps[op].fn(r, fp);
            if (!fp.done) {
                if (static_cast<int>(out.size()) <= count) {
                    out.push_back(fp);
                } else {
                    out[static_cast<std::size_t>(count)] = fp;
                }
                ++count;
            }
        } else {
            node = next;
        }
    }
    return r.failed() ? -1 : count;
}

} // namespace cyka::demo::ent
