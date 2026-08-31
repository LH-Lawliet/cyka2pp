// Huffman decode of field-path operations, ported from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/{huffman,field_path}.go. See NOTICE.

#include "cyka/demo/ent/field_path.hpp"

#include "cyka/demo/ent/field_path_ops.hpp"

#include <memory>
#include <queue>
#include <vector>

namespace cyka::demo::ent {
namespace {

inline constexpr std::size_t HUFFMAN_SIZE_FACTOR = 2;

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
    bool operator()(const BuildNode* lhs, const BuildNode* rhs) const noexcept {
        if (lhs->weight != rhs->weight) {
            return lhs->weight > rhs->weight;
        }
        return lhs->value < rhs->value;
    }
};

std::int16_t flattenTree(const BuildNode& root, std::vector<HuffNode>& out) {
    struct FlatFrame {
        const BuildNode* node;
        std::int16_t slot;
    };
    std::vector<FlatFrame> stack;
    const auto ROOT_IDX = static_cast<std::int16_t>(out.size());
    out.emplace_back();
    stack.push_back({.node = &root, .slot = ROOT_IDX});
    while (!stack.empty()) {
        const auto FRAME = stack.back();
        stack.pop_back();
        const auto IDX = FRAME.slot;
        if (FRAME.node->leaf()) {
            out[static_cast<std::size_t>(IDX)].left = -1;
            out[static_cast<std::size_t>(IDX)].value = static_cast<std::int16_t>(FRAME.node->value);
            continue;
        }
        const auto LEFT_IDX = static_cast<std::int16_t>(out.size());
        out.emplace_back();
        const auto RIGHT_IDX = static_cast<std::int16_t>(out.size());
        out.emplace_back();
        out[static_cast<std::size_t>(IDX)].left = LEFT_IDX;
        out[static_cast<std::size_t>(IDX)].right = RIGHT_IDX;
        stack.push_back({.node = FRAME.node->right.get(), .slot = RIGHT_IDX});
        stack.push_back({.node = FRAME.node->left.get(), .slot = LEFT_IDX});
    }
    return ROOT_IDX;
}

const std::vector<HuffNode>& huffNodes() {
    static const std::vector<HuffNode> NODES = [] {
        std::vector<std::unique_ptr<BuildNode>> owned;
        std::priority_queue<BuildNode*, std::vector<BuildNode*>, WorseThan> heap;
        for (std::size_t idx = 0; idx < FIELD_PATH_OPS.size(); ++idx) {
            auto leaf = std::make_unique<BuildNode>();
            leaf->weight = FIELD_PATH_OPS[idx].weight == 0 ? 1 : FIELD_PATH_OPS[idx].weight;
            leaf->value = static_cast<int>(idx);
            heap.push(leaf.get());
            owned.push_back(std::move(leaf));
        }
        int next_value = static_cast<int>(FIELD_PATH_OPS.size());
        while (heap.size() > 1) {
            const BuildNode* lhs = heap.top();
            heap.pop();
            const BuildNode* rhs = heap.top();
            heap.pop();
            auto parent = std::make_unique<BuildNode>();
            parent->weight = lhs->weight + rhs->weight;
            parent->value = next_value++;
            // Ownership transfer: `owned` keeps the arena alive until flattenTree().
            for (auto& node_ptr : owned) {
                if (node_ptr.get() == lhs) {
                    parent->left = std::move(node_ptr);
                } else if (node_ptr.get() == rhs) {
                    parent->right = std::move(node_ptr);
                }
            }
            heap.push(parent.get());
            owned.push_back(std::move(parent));
        }
        std::vector<HuffNode> flat;
        flat.reserve(HUFFMAN_SIZE_FACTOR * FIELD_PATH_OPS.size());
        if (!owned.empty()) {
            (void)flattenTree(*owned.back(), flat);
        }
        return flat;
    }();
    return NODES;
}

} // namespace

int readFieldPaths(BitStream& reader, std::vector<FieldPath>& out) {
    const auto& nodes = huffNodes();
    if (nodes.empty()) {
        return 0;
    }
    FieldPath field_path;
    int count = 0;
    std::int16_t node = 0;
    // Source 2 packs at most a few hundred field updates per entity; the cap
    // only guards against a desynced stream looping forever.
    constexpr int MAX_UPDATES = 4096;
    while (!field_path.done && !reader.failed() && count < MAX_UPDATES) {
        const std::int16_t NEXT = reader.readBool() ? nodes[static_cast<std::size_t>(node)].right
                                                    : nodes[static_cast<std::size_t>(node)].left;
        if (NEXT < 0 || static_cast<std::size_t>(NEXT) >= nodes.size()) {
            return -1;
        }
        if (nodes[static_cast<std::size_t>(NEXT)].left < 0) {
            node = 0;
            const auto OP_IDX =
                static_cast<std::size_t>(nodes[static_cast<std::size_t>(NEXT)].value);
            if (OP_IDX >= FIELD_PATH_OPS.size()) {
                return -1;
            }
            FIELD_PATH_OPS[OP_IDX].fn(reader, field_path);
            if (!field_path.done) {
                if (static_cast<std::size_t>(count) >= out.size()) {
                    out.push_back(field_path);
                } else {
                    out[static_cast<std::size_t>(count)] = field_path;
                }
                ++count;
            }
        } else {
            node = NEXT;
        }
    }
    return reader.failed() ? -1 : count;
}

} // namespace cyka::demo::ent
