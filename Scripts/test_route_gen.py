"""Python 复刻 RouteGraphGenerator 的算法，用于批量验证约束是否总能满足。

不追求与 UE 的 FRandomStream 逐位一致（RNG 不同），只验证：
  层数 / 每层节点数 / 连通性 / 无环 / 无死路 / 任意路径 >= 5 场战斗 / 同种子确定性。
"""
import random
import sys

NUM_LAYERS = 10
MIN_NODES = 2
MAX_NODES = 4
NUM_SLOTS = 7
MIN_BATTLES = 5
BATTLE_CHANCE = 0.55
MAX_ATTEMPTS = 64

SAFE, BATTLE = 0, 1


class Node:
    __slots__ = ("id", "layer", "slot", "type", "next", "prev")

    def __init__(self, nid, layer, slot, ntype):
        self.id = nid
        self.layer = layer
        self.slot = slot
        self.type = ntype
        self.next = []
        self.prev = []


def add_node(nodes, layer, slot, ntype):
    n = Node(len(nodes), layer, slot, ntype)
    nodes.append(n)
    return n.id


def nearest(ids, nodes, slot, exclude):
    best, best_d = None, 1 << 30
    for i, nid in enumerate(ids):
        if nid in exclude:
            continue
        d = abs(nodes[nid].slot - slot)
        if d < best_d:
            best_d, best = d, i
    return best


def connect(nodes, upper, lower, rng):
    n, m = len(upper), len(lower)
    if not n or not m:
        return

    def add(a, b):
        if b not in nodes[a].next:
            nodes[a].next.append(b)
        if a not in nodes[b].prev:
            nodes[b].prev.append(a)

    if m == 1:
        for u in upper:
            add(u, lower[0])
        return
    if n == 1:
        for lo in lower:
            add(upper[0], lo)
        return

    # 阶梯式单调连边（无交叉）
    for i in range(n):
        lo = (i * m) // n
        hi = min(m - 1, ((i + 1) * m) // n)
        for j in range(lo, hi + 1):
            add(upper[i], lower[j])

    # 随机删边（两端度数都 >1）
    for _ in range(2):
        for i in range(n):
            if len(nodes[upper[i]].next) <= 1:
                continue
            for j in range(m):
                if len(nodes[upper[i]].next) <= 1:
                    break
                if len(nodes[lower[j]].prev) <= 1:
                    continue
                if lower[j] not in nodes[upper[i]].next:
                    continue
                if rng.random() < 0.30:
                    nodes[upper[i]].next.remove(lower[j])
                    nodes[lower[j]].prev.remove(upper[i])


def min_battles_table(nodes):
    table = [0] * len(nodes)
    for nid in range(len(nodes) - 1, -1, -1):
        n = nodes[nid]
        suffix = 0
        if n.next:
            suffix = min(table[x] for x in n.next)
        table[nid] = (1 if n.type == BATTLE else 0) + suffix
    return table


def enforce_min_battles(nodes, start):
    for _ in range(len(nodes)):
        table = min_battles_table(nodes)
        if table[start] >= MIN_BATTLES:
            return
        cur = start
        flipped = False
        while cur is not None:
            node = nodes[cur]
            if cur != start and node.type != BATTLE:
                node.type = BATTLE
                flipped = True
                break
            nxt, best = None, 1 << 30
            for x in node.next:
                if table[x] < best:
                    best, nxt = table[x], x
            cur = nxt
        if not flipped:
            return


def build(seed):
    rng = random.Random(seed)
    nodes = []
    layers = [[] for _ in range(NUM_LAYERS)]
    layers[0].append(add_node(nodes, 0, rng.randrange(NUM_SLOTS), SAFE))

    for layer in range(1, NUM_LAYERS - 1):
        count = rng.randint(MIN_NODES, MAX_NODES)
        pool = list(range(NUM_SLOTS))
        rng.shuffle(pool)
        for slot in sorted(pool[:count]):
            layers[layer].append(add_node(nodes, layer, slot, SAFE))

    layers[NUM_LAYERS - 1].append(add_node(nodes, NUM_LAYERS - 1, NUM_SLOTS // 2, BATTLE))

    for layer in range(NUM_LAYERS - 1):
        connect(nodes, layers[layer], layers[layer + 1], rng)

    for layer in range(1, NUM_LAYERS - 1):
        for nid in layers[layer]:
            nodes[nid].type = BATTLE if rng.random() < BATTLE_CHANCE else SAFE

    start, end = layers[0][0], layers[-1][0]
    nodes[end].type = BATTLE
    enforce_min_battles(nodes, start)
    return nodes, start, end


def validate(nodes, start, end):
    errors = []
    by_layer = {}
    for n in nodes:
        if not (0 <= n.layer < NUM_LAYERS):
            errors.append("layer oob")
        by_layer.setdefault(n.layer, []).append(n)

    for layer, ns in by_layer.items():
        slots = [n.slot for n in ns]
        if len(set(slots)) != len(slots):
            errors.append(f"dup slot layer {layer}")
        if layer in (0, NUM_LAYERS - 1):
            if len(ns) != 1:
                errors.append(f"edge layer {layer} count {len(ns)}")
        elif not (MIN_NODES <= len(ns) <= MAX_NODES):
            errors.append(f"layer {layer} count {len(ns)}")

    for n in nodes:
        for x in n.next:
            if nodes[x].layer != n.layer + 1:
                errors.append("cross-layer edge")
            if n.id not in nodes[x].prev:
                errors.append("prev desync")
        if n.id != end and not n.next:
            errors.append(f"dead end {n.id}")
        if n.id != start and not n.prev:
            errors.append(f"unreachable {n.id}")

    # 正向/反向可达
    seen = {start}
    stack = [start]
    while stack:
        for x in nodes[stack.pop()].next:
            if x not in seen:
                seen.add(x)
                stack.append(x)
    if len(seen) != len(nodes):
        errors.append("forward reachability")

    seen = {end}
    stack = [end]
    while stack:
        for x in nodes[stack.pop()].prev:
            if x not in seen:
                seen.add(x)
                stack.append(x)
    if len(seen) != len(nodes):
        errors.append("backward reachability")

    if min_battles_table(nodes)[start] < MIN_BATTLES:
        errors.append("min battles")

    # 交叉检查
    for a in nodes:
        for b in nodes:
            if a.layer != b.layer or a.slot >= b.slot:
                continue
            for x in a.next:
                for y in b.next:
                    if nodes[x].slot > nodes[y].slot:
                        errors.append(f"crossing edge {a.id}->{x} / {b.id}->{y}")
    return errors


def signature(nodes):
    return tuple((n.layer, n.slot, n.type, tuple(n.next)) for n in nodes)


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 500
    failures = 0
    stats = {"nodes": 0, "edges": 0, "minb": 0, "battles": 0}
    for seed in range(1, count + 1):
        nodes, start, end = build(seed)
        errors = validate(nodes, start, end)
        # 确定性
        nodes2, _, _ = build(seed)
        if signature(nodes) != signature(nodes2):
            errors.append("nondeterministic")

        stats["nodes"] += len(nodes)
        stats["edges"] += sum(len(n.next) for n in nodes)
        stats["minb"] += min_battles_table(nodes)[start]
        stats["battles"] += sum(1 for n in nodes if n.type == BATTLE)

        if errors:
            failures += 1
            if failures <= 5:
                print(f"seed {seed}: {errors[:4]}")

    print(f"seeds={count} failures={failures}")
    for k, v in stats.items():
        print(f"  avg {k} = {v / count:.2f}")


if __name__ == "__main__":
    main()
