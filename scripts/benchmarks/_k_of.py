import json, sys

def k_of(path):
    d = json.load(open(path))
    nodes = {n["id"]: (n["x"], n["y"]) for n in d["nodes"]}
    E = [(e["source"], e["target"]) for e in d["edges"]]
    def o(p, q, r):
        v = (q[0]-p[0])*(r[1]-p[1]) - (q[1]-p[1])*(r[0]-p[0])
        return 0 if abs(v) < 1e-9 else (1 if v > 0 else -1)
    def seg(a, b, c, d_):
        return o(a,b,c) != o(a,b,d_) and o(c,d_,a) != o(c,d_,b)
    cnt = [0]*len(E)
    for i in range(len(E)):
        for j in range(i+1, len(E)):
            a, b = E[i]; c, dd = E[j]
            if len({a, b, c, dd}) < 4:
                continue
            if seg(nodes[a], nodes[b], nodes[c], nodes[dd]):
                cnt[i] += 1; cnt[j] += 1
    return max(cnt) if cnt else 0

for path in sys.argv[1:]:
    print(f"{path.split('/')[-1]:14} k={k_of(path)}")
