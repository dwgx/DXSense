"""
dwrg.exe - 第五人格模仿者模式身份读取工具
pip install pymem
以管理员权限运行
"""

import sys
import os
import time

try:
    import pymem
    import pymem.process
except ImportError:
    print("请先安装: pip install pymem")
    sys.exit(1)

from scanner import _get_readable_regions

PROCESS_NAME = "dwrg.exe"

CAMP_NAMES = {1: "侦探团", 2: "模仿者", 3: "神秘客"}

IDENTITY_MAP = {
    # 侦探团 (camp 1)
    101: "侦探", 102: "哨兵", 103: "治安官", 104: "猎人",
    105: "香料师", 106: "锁匠", 107: "银行家", 108: "修士",
    109: "演说家", 110: "拳击手", 111: "灵媒", 112: "学徒",
    113: "密探", 114: "掮客", 115: "药剂师", 116: "巡林员",
    117: "执灯人", 118: "评论家",
    # 模仿者 (camp 2)
    201: "神偷", 202: "千面人", 203: "阴谋家", 204: "烟火师",
    205: "怪盗", 206: "催眠师", 207: "地下医生", 208: "处刑人",
    209: "指挥家",
    # 神秘客 (camp 3)
    301: "流浪汉", 302: "送货员", 303: "愚人", 304: "棋手",
    305: "清洁工", 306: "顾问", 307: "降灵师", 308: "导演",
}

# 普通模式角色名映射 (character_id / survivor_id / hunter_id → 角色名)
# ID 顺序按角色上线时间排列，不确定的 ID 留空后续通过 f 命令确认
SURVIVOR_NAMES = {
    1: "幸运儿",
    2: "医生",
    3: "律师",
    4: "冒险家",
    5: "慈善家",
    6: "园丁",
    7: "魔术师",
    8: "机械师",
    9: "前锋",
    10: "空军",
    11: "佣兵",
    12: "祭司",
    13: "调香师",
    14: "牛仔",
    15: "舞女",
    16: "先知",
    17: "盲女",
    18: "入殓师",
    19: "勘探员",
    20: "咒术师",
    21: "野人",
    22: "杂技演员",
    23: "大副",
    24: "调酒师",
    25: "邮差",
    26: "守墓人",
    27: "囚徒",
    28: "昆虫学者",
    29: "画家",
    30: "击球手",
    31: "玩具商",
    32: "心理学家",
    33: "患者",
    34: "小说家",
    35: "小女孩",
    36: "哭泣小丑",
    37: "教授",
    38: "古董商",
    39: "作曲家",
    40: "记者",
    41: "飞行家",
    42: "拉拉队员",
    43: "木偶师",
    44: "火灾调查员",
    45: "涅墨女士",
    46: "骑士",
    47: "气象学家",
    48: "弓箭手",
    49: "逃脱大师",
    50: "幻灯师",
    51: "斗牛士",
}
HUNTER_NAMES = {
    1: "厂长",
    2: "小丑",
    3: "鹿头",
    4: "杰克",
    5: "蜘蛛",
    6: "红蝶",
    7: "黄衣之主",
    8: "宿伞之魂",
    9: "摄影师",
    10: "疯眼",
    11: "梦之女巫",
    12: "爱哭鬼",
    13: "寄蝶",
    14: "红夫人",
    15: "26号守卫",
    16: "使徒",
    17: "小提琴家",
    18: "雕刻家",
    19: "博士",
    20: "破轮",
    21: "渔女",
    22: "蜡像师",
    23: "噩梦",
    24: "记录员",
    25: "隐士",
    26: "守夜人",
    27: "歌剧演员",
    28: "愚人金",
    29: "时空之影",
    30: "跛脚羊",
    31: "瘟疫",
    32: "杂货商",
    33: "台球手",
    34: "女王蜂",
}

# ── msgpack 解析 ──────────────────────────────────────

def _read_msgpack_int(data, pos):
    """读取 pos 处的 msgpack 整数值，返回 (value, bytes_consumed) 或 None"""
    if pos >= len(data):
        return None
    b = data[pos]
    if b <= 0x7F:
        return (b, 1)
    if b >= 0xE0:
        return (b - 256, 1)
    if b == 0xCC and pos + 1 < len(data):
        return (data[pos + 1], 2)
    if b == 0xCD and pos + 2 < len(data):
        return (int.from_bytes(data[pos + 1:pos + 3], 'big'), 3)
    return None


def _find_field(data, key_bytes, search_start, search_end, last=False):
    """在 data[search_start:search_end] 中找 msgpack key 并读取其整数值
    last=True 时找最后一个匹配（用于向前搜索，避免找到相邻玩家的字段）"""
    region = data[search_start:search_end]
    pos = region.rfind(key_bytes) if last else region.find(key_bytes)
    if pos < 0:
        return -1
    val_pos = search_start + pos + len(key_bytes)
    result = _read_msgpack_int(data, val_pos)
    return result[0] if result else -1


def _parse_ci_match(data, idx):
    """
    从 copycat_index 匹配位置解析玩家数据。
    支持两种字段顺序：
      A) identity_id -> camp_id -> copycat_index (玩家列表)
      B) copycat_index -> camp_id -> identity_id (mode_info)
    返回 (copycat_index, camp_id, identity_id) 或 None
    """
    val_pos = idx + 14  # skip \xad (1) + 'copycat_index' (13)
    result = _read_msgpack_int(data, val_pos)
    if not result:
        return None
    ci_val = result[0]

    camp = -1
    ident = -1

    # 先往后找 (格式B)
    after_start = val_pos + result[1]
    after_end = min(after_start + 80, len(data))
    camp = _find_field(data, b'\xa7camp_id', after_start, after_end)
    ident = _find_field(data, b'\xabidentity_id', after_start, after_end)

    # 没找全就往前找 (格式A) — 用 last=True 找最近的，避免找到上一个玩家的字段
    if camp <= 0 or ident <= 0:
        before_start = max(0, idx - 80)
        if camp <= 0:
            camp = _find_field(data, b'\xa7camp_id', before_start, idx, last=True)
        if ident <= 0:
            ident = _find_field(data, b'\xabidentity_id', before_start, idx, last=True)

    if 1 <= ci_val <= 12 and camp > 0 and ident > 0:
        return (ci_val, camp, ident)
    return None


def _parse_ident_match(data, idx):
    """
    从 identity_id 匹配位置正向解析 (格式A: identity_id -> camp_id -> copycat_index)
    返回 (copycat_index, camp_id, identity_id, ci_offset_in_data) 或 None
    """
    val_pos = idx + 12  # skip \xab (1) + 'identity_id' (11)
    result = _read_msgpack_int(data, val_pos)
    if not result:
        return None
    ident = result[0]
    if ident <= 0:
        return None

    after_start = val_pos + result[1]
    after_end = min(after_start + 120, len(data))
    camp = _find_field(data, b'\xa7camp_id', after_start, after_end)
    if camp <= 0:
        return None

    # 找 copycat_index 的位置（需要返回偏移用于去重）
    region = data[after_start:after_end]
    ci_key = b'\xadcopycat_index'
    ci_pos = region.find(ci_key)
    if ci_pos < 0:
        return None
    ci_abs_pos = after_start + ci_pos
    ci_result = _read_msgpack_int(data, ci_abs_pos + 14)
    if not ci_result:
        return None
    ci_val = ci_result[0]

    if 1 <= ci_val <= 12 and camp > 0 and ident > 0:
        return (ci_val, camp, ident, ci_abs_pos)
    return None


# -- 内存扫描 --

def _scan_all_ci(pm, debug=False):
    """
    扫描全内存，返回所有有效的 (ci, camp, ident, addr, is_playerlist)。
    is_playerlist=True 表示格式A（带 uid/player_name 的玩家列表），最可靠。
    """
    regions = _get_readable_regions(pm.process_handle)
    CHUNK = 0x400000
    OVERLAP = 256
    target_ci = b'\xadcopycat_index'
    target_id = b'\xabidentity_id'
    entries = []
    seen_ci_addrs = set()
    total_ci = 0
    parsed_ok = 0
    total_id = 0
    parsed_id = 0

    if debug:
        total_mem = sum(s for _, s in regions)
        print("[DEBUG] 内存区域: %d 个, 总大小: %.1f MB" % (len(regions), total_mem / 1048576))

    for base, size in regions:
        for off in range(0, size, CHUNK):
            # 往前多读 OVERLAP 字节（如果不是第一块）
            pre = min(OVERLAP, off)
            read_start = base + off - pre
            read_size = min(CHUNK + pre + OVERLAP, size - off + pre)
            if read_size <= 0:
                continue
            try:
                d = pm.read_bytes(read_start, read_size)
            except Exception:
                continue
            # 只在 [pre, pre+CHUNK) 范围内搜索，避免重复
            search_lo = pre
            search_hi = min(pre + CHUNK, len(d))

            # ── 锚点1: copycat_index (处理格式A和B) ──
            pos = search_lo
            while True:
                idx = d.find(target_ci, pos, search_hi)
                if idx == -1:
                    break
                total_ci += 1
                result = _parse_ci_match(d, idx)
                if result:
                    parsed_ok += 1
                    ci, camp, ident = result
                    abs_addr = read_start + idx
                    seen_ci_addrs.add(abs_addr)
                    # 检测格式A签名：copycat_index 值后紧跟 \xb5 (hide_role_select_name)
                    val_end = idx + 15  # \xad(1) + 'copycat_index'(13) + fixint(1)
                    is_pl = (val_end < len(d) and d[val_end] == 0xB5)
                    entries.append((ci, camp, ident, abs_addr, is_pl))
                pos = idx + 1

            # ── 锚点2: identity_id 正向解析 (专门捕获格式A) ──
            pos = search_lo
            while True:
                idx = d.find(target_id, pos, search_hi)
                if idx == -1:
                    break
                total_id += 1
                result = _parse_ident_match(d, idx)
                if result:
                    ci, camp, ident, ci_off = result
                    abs_ci_addr = read_start + ci_off
                    if abs_ci_addr not in seen_ci_addrs:
                        seen_ci_addrs.add(abs_ci_addr)
                        parsed_id += 1
                        # 检测格式A签名：copycat_index 值后紧跟 \xb5 (hide_role_select_name)
                        val_end = ci_off + 15  # \xad(1) + 'copycat_index'(13) + fixint(1)
                        is_pl = (val_end < len(d) and d[val_end] == 0xB5)
                        entries.append((ci, camp, ident, abs_ci_addr, is_pl))
                pos = idx + 1

    if debug:
        print("[DEBUG] 锚点1(copycat_index): %d 匹配, %d 解析成功" % (total_ci, parsed_ok))
        print("[DEBUG] 锚点2(identity_id):   %d 匹配, %d 新增条目" % (total_id, parsed_id))
        print("[DEBUG] 总条目: %d" % len(entries))
        print("[DEBUG] 每条详情:")
        for i, (ci, camp, ident, addr, is_pl) in enumerate(entries):
            fmt = "格式A" if is_pl else "格式B"
            print("  [%2d] ci=%-2d camp=%-3d ident=%-3d addr=0x%X %s %s" % (
                i, ci, camp, ident, addr, fmt, IDENTITY_MAP.get(ident, "?")))

    return entries


def _cluster_entries(entries, gap=0x10000):
    """按地址聚类，间距 < gap 的归为一组"""
    if not entries:
        return []
    entries.sort(key=lambda x: x[3])
    clusters = [[entries[0]]]
    for e in entries[1:]:
        if e[3] - clusters[-1][-1][3] < gap:
            clusters[-1].append(e)
        else:
            clusters.append([e])
    return clusters


def read_identities(pm, debug=False, prev_result=None):
    """
    扫描内存，优先使用格式A（带uid的玩家列表）数据。
    如果有多个格式A簇，选和上一局结果不同的那个（=新一局）。
    返回 [(copycat_index, camp_id, identity_id), ...] 按 index 排序。
    """
    from collections import Counter

    entries = _scan_all_ci(pm, debug=debug)
    if not entries:
        print("[X] 未找到玩家数据")
        return []

    # 分离格式A（玩家列表）和其他格式
    pl_entries = [e for e in entries if e[4]]
    other_entries = [e for e in entries if not e[4]]

    if debug:
        print("[DEBUG] 共 %d 条匹配 (格式A: %d, 其他: %d)" % (
            len(entries), len(pl_entries), len(other_entries)))
        for i, (ci, camp, ident, addr, is_pl) in enumerate(entries):
            fmt = "A" if is_pl else "B"
            print("  [%2d] ci=%-2d camp=%-3d ident=%-3d 0x%X %s %s" % (
                i, ci, camp, ident, addr, fmt, IDENTITY_MAP.get(ident, "?")))

    # 格式A聚类
    best = None
    if pl_entries:
        pl_4col = [(ci, camp, ident, addr) for ci, camp, ident, addr, _ in pl_entries]
        pl_clusters = _cluster_entries(pl_4col)

        if debug:
            print("[DEBUG] 格式A: %d 个簇" % len(pl_clusters))
            for i, cl in enumerate(pl_clusters):
                uci = set(e[0] for e in cl)
                print("  簇%d: %d条 %d人 @ 0x%X: %s" % (
                    i, len(cl), len(uci), cl[0][3],
                    ", ".join("%d=%s" % (ci, IDENTITY_MAP.get(ident, "?"))
                              for ci, camp, ident in sorted(set((x[0], x[1], x[2]) for x in cl)))))

        # 把每个簇转成身份集合，用于和上一局比较
        def cluster_to_set(cl):
            by_idx = {}
            for ci, camp, ident, _ in cl:
                by_idx.setdefault(ci, []).append((camp, ident))
            result = set()
            for ci in by_idx:
                votes = Counter(by_idx[ci])
                best_vote = votes.most_common(1)[0][0]
                result.add((ci, best_vote[0], best_vote[1]))
            return result

        prev_set = set(prev_result) if prev_result else set()

        # 按玩家数降序排列
        pl_clusters.sort(key=lambda cl: len(set(e[0] for e in cl)), reverse=True)

        if prev_set and len(pl_clusters) > 1:
            # 有上一局数据，优先选和上一局不同的簇
            for cl in pl_clusters:
                cl_set = cluster_to_set(cl)
                if len(set(e[0] for e in cl)) >= 4 and cl_set != prev_set:
                    best = cl
                    if debug:
                        print("[DEBUG] 选择了与上一局不同的簇 (新一局)")
                    break

        if not best:
            best = pl_clusters[0]  # 最大的簇

    # 格式A数据不足4人时，用全部数据兜底
    if not best or len(set(e[0] for e in best)) < 4:
        if debug:
            print("[DEBUG] 格式A数据不足，使用全部数据聚类")
        all_4col = [(ci, camp, ident, addr) for ci, camp, ident, addr, _ in entries]
        all_clusters = _cluster_entries(all_4col)
        if all_clusters:
            candidate = max(all_clusters, key=lambda cl: len(set(e[0] for e in cl)))
            if len(set(e[0] for e in candidate)) >= 4:
                best = candidate
            else:
                # 聚类失败，尝试把所有条目当作一个整体（格式B分散在不同区域）
                unique_cis = set(e[0] for e in all_4col)
                if len(unique_cis) >= 4:
                    if debug:
                        print("[DEBUG] 聚类不足4人但总共有%d人，合并所有条目" % len(unique_cis))
                    best = all_4col
                else:
                    if debug:
                        print("[DEBUG] 总共也不足4人，放弃（可能不在对局中）")
                    best = None
        else:
            best = None

    if not best:
        print("[X] 数据不足，可能不在对局中或对局刚开始")
        return []

    # 簇内去重
    by_idx = {}
    for ci, camp, ident, _ in best:
        by_idx.setdefault(ci, []).append((camp, ident))

    unique = []
    for ci in sorted(by_idx):
        votes = Counter(by_idx[ci])
        camp, ident = votes.most_common(1)[0][0]
        unique.append((ci, camp, ident))

    print("[i] 找到 %d 个玩家" % len(unique))
    return unique


# -- 显示 --

def display(players):
    """格式化显示玩家身份"""
    print()
    print("=" * 56)
    print("        第五人格 模仿者模式 - 身份一览")
    print("=" * 56)

    for idx, camp, ident in players:
        camp_name = CAMP_NAMES.get(camp, "???")
        role_name = IDENTITY_MAP.get(ident, "未知(ID=%d)" % ident)
        icons = {1: "[侦探团]", 2: "[模仿者]", 3: "[神秘客]"}
        icon = icons.get(camp, "[???]")
        print("  玩家 %2d:  %-8s  阵营=%s  角色=%s" % (idx, icon, camp_name, role_name))

    print("=" * 56)

    groups = {1: [], 2: [], 3: []}
    for p in players:
        groups.setdefault(p[1], []).append(p)

    labels = {2: "模仿者", 3: "神秘客", 1: "侦探团"}
    print()
    for camp_id in [2, 3, 1]:
        g = groups.get(camp_id, [])
        if g:
            names = ", ".join("#%d(%s)" % (p[0], IDENTITY_MAP.get(p[2], "?")) for p in g)
            print("  %s: %s" % (labels[camp_id], names))
    print()


def _dump_format_a_region(pm):
    """找到格式A条目，读取其周围大块内存，搜索所有玩家记录"""
    # 先找格式A地址
    entries = _scan_all_ci(pm, debug=True)
    pl = [(ci, camp, ident, addr) for ci, camp, ident, addr, is_pl in entries if is_pl]
    if not pl:
        print("[X] 未找到格式A条目")
        return

    print("\n[格式A条目]:")
    for ci, camp, ident, addr in pl:
        print("  ci=%d addr=0x%X %s" % (ci, addr, IDENTITY_MAP.get(ident, "?")))

    # 读取每个格式A地址前后4KB
    for ci, camp, ident, addr in pl:
        start = addr - 2048
        try:
            block = pm.read_bytes(start, 8192)
        except Exception:
            continue
        print("\n=== 格式A ci=%d 周围 8KB @ 0x%X ===" % (ci, start))
        # 搜索所有 identity_id 出现
        target = b'\xabidentity_id'
        pos = 0
        while True:
            idx = block.find(target, pos)
            if idx == -1:
                break
            val_pos = idx + 12
            result = _read_msgpack_int(block, val_pos)
            if result:
                ident_val = result[0]
                # 继续找 camp_id 和 copycat_index
                after = val_pos + result[1]
                camp_val = _find_field(block, b'\xa7camp_id', after, min(after + 40, len(block)))
                ci_val = _find_field(block, b'\xadcopycat_index', after, min(after + 60, len(block)))
                abs_addr = start + idx
                name = IDENTITY_MAP.get(ident_val, "?")
                print("  identity_id=%d(%s) camp=%d ci=%d @ 0x%X" % (
                    ident_val, name, camp_val, ci_val, abs_addr))
            pos = idx + 1


# -- 原始dump --

def _dump_raw_matches(pm):
    """dump 每个 copycat_index 匹配点前后的原始字节"""
    regions = _get_readable_regions(pm.process_handle)
    target = b'\xadcopycat_index'
    CHUNK = 0x400000
    OVERLAP = 256
    match_num = 0

    for base, size in regions:
        for off in range(0, size, CHUNK):
            pre = min(OVERLAP, off)
            read_start = base + off - pre
            read_size = min(CHUNK + pre + OVERLAP, size - off + pre)
            if read_size <= 0:
                continue
            try:
                d = pm.read_bytes(read_start, read_size)
            except Exception:
                continue
            search_lo = pre
            search_hi = min(pre + CHUNK, len(d))
            pos = search_lo
            while True:
                idx = d.find(target, pos, search_hi)
                if idx == -1:
                    break
                match_num += 1
                abs_addr = read_start + idx
                # 取前100后150字节
                lo = max(0, idx - 100)
                hi = min(len(d), idx + 150)
                chunk = d[lo:hi]
                print("\n--- 匹配 #%d @ 0x%X ---" % (match_num, abs_addr))
                for r in range(0, len(chunk), 32):
                    row = chunk[r:r + 32]
                    hex_part = " ".join("%02X" % b for b in row)
                    asc_part = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
                    marker = " <<< CI" if (lo + r <= idx < lo + r + 32) else ""
                    print("  %s  %s%s" % (hex_part, asc_part, marker))
                pos = idx + 1

    print("\n[i] 共 %d 个匹配" % match_num)


# -- msgpack 工具 --

def _encode_msgpack_key(name_bytes):
    """将字段名编码为 msgpack fixstr/str8 格式"""
    length = len(name_bytes)
    if length < 32:
        return bytes([0xA0 | length]) + name_bytes
    elif length < 256:
        return bytes([0xD9, length]) + name_bytes
    return None


def _read_msgpack_value(data, pos):
    """读取 pos 处的 msgpack 值，返回 (type_str, value, bytes_consumed) 或 None"""
    if pos >= len(data):
        return None
    b = data[pos]
    # bool
    if b == 0xC3:
        return ("bool", True, 1)
    if b == 0xC2:
        return ("bool", False, 1)
    # nil
    if b == 0xC0:
        return ("nil", None, 1)
    # positive fixint
    if b <= 0x7F:
        return ("int", b, 1)
    # negative fixint
    if b >= 0xE0:
        return ("int", b - 256, 1)
    # uint8
    if b == 0xCC and pos + 1 < len(data):
        return ("uint8", data[pos + 1], 2)
    # uint16
    if b == 0xCD and pos + 2 < len(data):
        return ("uint16", int.from_bytes(data[pos + 1:pos + 3], 'big'), 3)
    # uint32
    if b == 0xCE and pos + 4 < len(data):
        return ("uint32", int.from_bytes(data[pos + 1:pos + 5], 'big'), 5)
    # int8
    if b == 0xD0 and pos + 1 < len(data):
        v = data[pos + 1]
        return ("int8", v - 256 if v > 127 else v, 2)
    # int16
    if b == 0xD1 and pos + 2 < len(data):
        return ("int16", int.from_bytes(data[pos + 1:pos + 3], 'big', signed=True), 3)
    # int32
    if b == 0xD2 and pos + 4 < len(data):
        return ("int32", int.from_bytes(data[pos + 1:pos + 5], 'big', signed=True), 5)
    # float32
    if b == 0xCA and pos + 4 < len(data):
        import struct
        return ("float32", struct.unpack(">f", data[pos + 1:pos + 5])[0], 5)
    # float64
    if b == 0xCB and pos + 8 < len(data):
        import struct
        return ("float64", struct.unpack(">d", data[pos + 1:pos + 9])[0], 9)
    # fixstr
    if 0xA0 <= b <= 0xBF:
        slen = b & 0x1F
        if pos + 1 + slen <= len(data):
            try:
                s = data[pos + 1:pos + 1 + slen].decode("utf-8", errors="replace")
            except Exception:
                s = repr(data[pos + 1:pos + 1 + slen])
            return ("str", s, 1 + slen)
    # str8
    if b == 0xD9 and pos + 1 < len(data):
        slen = data[pos + 1]
        if pos + 2 + slen <= len(data):
            try:
                s = data[pos + 2:pos + 2 + slen].decode("utf-8", errors="replace")
            except Exception:
                s = repr(data[pos + 2:pos + 2 + slen])
            return ("str", s, 2 + slen)
    return None


def _extract_msgpack_keys(data, start, end):
    """从 data[start:end] 提取所有 msgpack string key -> value 对"""
    results = []
    pos = start
    while pos < end:
        b = data[pos]
        key_name = None
        key_len = 0
        # fixstr key
        if 0xA0 <= b <= 0xBF:
            slen = b & 0x1F
            if slen > 0 and pos + 1 + slen <= end:
                try:
                    key_name = data[pos + 1:pos + 1 + slen].decode("ascii")
                except Exception:
                    pos += 1
                    continue
                key_len = 1 + slen
        # str8 key
        elif b == 0xD9 and pos + 1 < end:
            slen = data[pos + 1]
            if slen > 0 and pos + 2 + slen <= end:
                try:
                    key_name = data[pos + 2:pos + 2 + slen].decode("ascii")
                except Exception:
                    pos += 1
                    continue
                key_len = 2 + slen

        if key_name and key_name.isprintable() and len(key_name) >= 2:
            val_pos = pos + key_len
            val_info = _read_msgpack_value(data, val_pos)
            if val_info:
                results.append((key_name, val_info[0], val_info[1], pos))
        pos += 1
    return results


# -- 字段发现 --

def _discover_fields(pm):
    """发现玩家数据附近的所有 msgpack 字段名"""
    entries = _scan_all_ci(pm, debug=False)
    pl = [e for e in entries if e[4]]  # 格式A
    if not pl:
        pl = entries
    if not pl:
        print("[X] 未找到玩家数据")
        return

    # 取前几个不同 ci 的条目
    seen_ci = set()
    samples = []
    for ci, camp, ident, addr, _ in pl:
        if ci not in seen_ci and len(samples) < 3:
            seen_ci.add(ci)
            samples.append((ci, camp, ident, addr))

    all_keys = {}  # key_name -> [(type, value, ci), ...]

    for ci, camp, ident, addr in samples:
        # 读取 copycat_index 前后 512 字节
        read_start = addr - 512
        try:
            block = pm.read_bytes(read_start, 1024)
        except Exception:
            continue
        keys = _extract_msgpack_keys(block, 0, len(block))
        for key_name, val_type, val, _ in keys:
            all_keys.setdefault(key_name, []).append((val_type, val, ci))

    if not all_keys:
        print("[X] 未发现任何字段")
        return

    # 按出现次数排序
    print("\n" + "=" * 70)
    print("  发现的 msgpack 字段 (玩家数据附近)")
    print("=" * 70)
    for key_name in sorted(all_keys, key=lambda k: (-len(all_keys[k]), k)):
        entries_list = all_keys[key_name]
        # 显示每个玩家的值
        vals = []
        for vt, vv, ci in entries_list:
            if vt.startswith("float"):
                vals.append("ci%d=%.2f" % (ci, vv))
            elif vt == "bool":
                vals.append("ci%d=%s" % (ci, vv))
            elif vt == "str":
                vals.append("ci%d='%s'" % (ci, vv[:20]))
            else:
                vals.append("ci%d=%s" % (ci, vv))
        print("  %-30s [%s] %s" % (key_name, entries_list[0][0], ", ".join(vals)))
    print("=" * 70)
    print("[i] 共 %d 个不同字段" % len(all_keys))
    return all_keys


# -- 自动识别"我是谁" --

def _detect_self(pm):
    """
    尝试识别当前玩家的 copycat_index。
    策略：搜索 is_self/is_local 等 bool=true 字段，或让用户手动指定。
    返回 copycat_index (int) 或 None
    """
    entries = _scan_all_ci(pm, debug=False)
    pl = [e for e in entries if e[4]]
    if not pl:
        return None

    # 取每个 ci 的一个地址
    ci_addrs = {}
    for ci, camp, ident, addr, _ in pl:
        if ci not in ci_addrs:
            ci_addrs[ci] = (addr, camp, ident)

    # 在每个玩家数据附近搜索 is_self / is_local / is_me 等 bool=true 字段
    self_keywords = [b"is_self", b"is_local", b"is_me", b"is_mine", b"self_player",
                     b"local_player", b"is_owner"]
    for ci in sorted(ci_addrs):
        addr, camp, ident = ci_addrs[ci]
        try:
            block = pm.read_bytes(addr - 256, 512)
        except Exception:
            continue
        for kw in self_keywords:
            encoded = _encode_msgpack_key(kw)
            if not encoded:
                continue
            idx = block.find(encoded)
            if idx >= 0:
                val_pos = idx + len(encoded)
                if val_pos < len(block) and block[val_pos] == 0xC3:  # true
                    return ci

    return None


# -- 智能黑灯移除 --

def _smart_blackout(pm, players, self_ci=None):
    """
    在玩家数据附近搜索所有 bool=true 的字段，列出让用户选择哪个是黑灯。
    如果自己是模仿者则跳过。
    """
    # 检查自己是否是模仿者
    if self_ci and players:
        for ci, camp, ident in players:
            if ci == self_ci and camp == 2:
                print("[i] 你是模仿者 (玩家#%d)，不需要移除黑灯特效" % ci)
                return

    # 加载已保存的黑灯字段名
    config = None
    config_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")
    try:
        import json
        if os.path.exists(config_path):
            with open(config_path, "r", encoding="utf-8") as f:
                config = json.load(f)
    except Exception:
        pass

    saved_field = config.get("blackout_field") if config else None

    entries = _scan_all_ci(pm, debug=False)
    pl = [e for e in entries if e[4]]
    if not pl:
        print("[X] 未找到玩家数据")
        return

    # 收集所有玩家数据附近的 bool 字段
    bool_fields = {}  # field_name -> [(addr, value, ci), ...]
    seen_ci = set()
    for ci, camp, ident, addr, _ in pl:
        if ci in seen_ci:
            continue
        seen_ci.add(ci)
        try:
            block = pm.read_bytes(addr - 512, 1024)
        except Exception:
            continue
        keys = _extract_msgpack_keys(block, 0, len(block))
        for key_name, val_type, val, offset in keys:
            if val_type == "bool":
                abs_addr = (addr - 512) + offset + len(_encode_msgpack_key(key_name.encode("utf-8")))
                bool_fields.setdefault(key_name, []).append((abs_addr, val, ci))

    if not bool_fields:
        print("[X] 未找到 bool 类型字段")
        return

    # 如果有保存的字段名，直接使用
    if saved_field and saved_field in bool_fields:
        print("[i] 使用已保存的黑灯字段: %s" % saved_field)
        _write_bool_false(pm, saved_field, bool_fields[saved_field])
        return

    # 列出所有 bool 字段
    true_fields = {k: v for k, v in bool_fields.items() if any(val for _, val, _ in v)}
    if not true_fields:
        print("[i] 所有 bool 字段都是 false，黑灯可能未触发")
        return

    print("\n[i] 发现 %d 个值为 true 的 bool 字段:" % len(true_fields))
    field_list = sorted(true_fields.keys())
    for i, name in enumerate(field_list):
        entries_list = true_fields[name]
        cis = [ci for _, _, ci in entries_list]
        print("  [%d] %-30s (出现在玩家: %s)" % (i, name, ", ".join("#%d" % c for c in cis)))

    try:
        choice = input("\n输入编号选择黑灯字段 (s=保存选择, 回车取消): ").strip()
    except (EOFError, KeyboardInterrupt):
        return

    if not choice:
        return

    save = False
    if choice.endswith("s"):
        save = True
        choice = choice[:-1].strip()

    try:
        idx = int(choice)
        field_name = field_list[idx]
    except (ValueError, IndexError):
        print("[X] 无效选择")
        return

    _write_bool_false(pm, field_name, true_fields[field_name])

    if save:
        try:
            import json
            cfg = config or {}
            cfg["blackout_field"] = field_name
            with open(config_path, "w", encoding="utf-8") as f:
                json.dump(cfg, f, indent=2, ensure_ascii=False)
            print("[OK] 已保存字段名 '%s' 到 config.json" % field_name)
        except Exception as e:
            print("[X] 保存失败: %s" % e)


def _write_bool_false(pm, field_name, entries_list):
    """将指定字段的所有 true 值改为 false"""
    count = 0
    for addr, val, ci in entries_list:
        if val is True:
            try:
                pm.write_bytes(addr, bytes([0xC2]), 1)
                count += 1
            except Exception:
                pass
    print("[OK] %s: 已将 %d 个 true 改为 false" % (field_name, count))


# -- 地图与房间定义 --

MAP_NAMES = {
    1: "白沙疯人院",
    2: "军工厂",
    3: "圣心医院",
    5: "红教堂",
    8: "月亮河公园",
    9: "湖景村",
    10: "永眠镇",
    11: "唐人街",
    12: "里奥的回忆",
    13: "白沙街疯人院",
    14: "闪金石窟",
    18: "圣心医院",
    30: "唐人街",
    81: "白沙疯人院",
    82: "闪金石窟",
}

# 房间区域定义: (x_min, x_max, z_min, z_max) -> 房间名
# 坐标范围基于实际游戏数据，x约-300~300, z约-200~550
# 用 'cal' 命令在游戏中校准精确坐标
ROOM_DEFS = {
    "白沙疯人院": [
        # 左上区域
        ((-300, -100, -200, 0), "西沐浴间"),
        ((-300, -100, 0, 100), "花园"),
        ((-300, -100, 100, 200), "餐厅"),
        ((-300, -100, 200, 400), "寝室"),
        # 中间区域
        ((-100, 0, -200, -50), "盥洗室"),
        ((-100, 0, -50, 50), "电力室"),
        ((-100, 0, 50, 150), "娱乐室"),
        ((-100, 0, 150, 300), "祷告堂"),
        # 右侧区域
        ((0, 100, -200, -50), "东沐浴间"),
        ((0, 100, -50, 50), "杂物间"),
        ((0, 100, 50, 150), "走廊"),
        ((0, 100, 150, 300), "监控室"),
        # 远右区域
        ((100, 300, -200, 100), "庭院"),
        ((100, 300, 100, 300), "办公区"),
        # 远处区域
        ((-300, 300, 300, 600), "外围"),
    ],
    "闪金石窟": [
        # 左上区域
        ((-300, -100, -200, 0), "木材堆放区"),
        ((-300, -100, 0, 100), "熔炉房"),
        ((-300, -100, 100, 200), "宿舍"),
        ((-300, -100, 200, 400), "通讯室"),
        # 中间区域
        ((-100, 0, -200, -50), "废墟"),
        ((-100, 0, -50, 50), "矿物检测室"),
        ((-100, 0, 50, 150), "电梯房"),
        ((-100, 0, 150, 300), "盥洗室"),
        # 右侧区域
        ((0, 100, -200, -50), "采矿区"),
        ((0, 100, -50, 50), "矿车维修区"),
        ((0, 100, 50, 150), "巨石堆"),
        ((0, 100, 150, 300), "食堂"),
        # 远右区域
        ((100, 300, -200, 100), "碎石区"),
        ((100, 300, 100, 300), "办公区"),
    ],
}

# 演绎要求（任务）列表
TASK_DEFS = {
    "白沙疯人院": {
        "寝室": ["整理床铺"],
        "祷告堂": ["鉴别画作"],
        "电力室": ["连接线路"],
        "东沐浴间": ["清洁地面"],
        "西沐浴间": ["寻找抹布"],
        "餐厅": ["摆放餐具"],
        "花园": ["清理杂草", "点亮蜡烛", "捕捉萤火虫"],
        "盥洗室": ["清理污水"],
        "杂物间": ["寻找餐具"],
        "监控室": ["连接线路"],
        "娱乐室": ["校准时钟", "弹钢琴", "连线"],
    },
    "闪金石窟": {
        "宿舍": ["拍虫子"],
        "盥洗室": ["清理污水"],
        "电梯房": ["电梯维修"],
        "食堂": ["点亮蜡烛", "捉老鼠"],
        "通讯室": ["维修通讯信号"],
        "熔炉房": ["电力回复", "熔炉控温"],
        "矿物检测室": ["摆放矿石", "矿物分类"],
        "矿车维修区": ["修理矿车"],
        "巨石堆": ["矿石装车"],
        "木材堆放区": ["清理杂草"],
        "废墟": ["连接线路", "捉萤火虫"],
        "采矿区": ["驱赶蝙蝠", "捉老鼠", "每日检查"],
        "碎石区": ["砸碎石头", "摧毁蚁穴", "找出易碎位置", "挖掘矿石"],
        "办公区": ["图纸作业", "拿取矿石"],
    },
}

TASK_GOAL = 55  # 侦探团胜利需要完成的演绎要求数

# 可能的任务进度相关字段名（由 'f' 命令发现后可更新）
TASK_FIELD_CANDIDATES = [
    b"task_count", b"task_num", b"task_done", b"task_complete",
    b"finish_task", b"finished_task", b"complete_task",
    b"deduction_count", b"deduction_num", b"deduction_done",
    b"mission_count", b"mission_done", b"mission_num",
    b"quest_count", b"quest_done", b"quest_num",
    "演绎".encode("utf-8"), b"task", b"progress",
]


def _coord_to_room(map_name, x, z):
    """将坐标映射到房间名 (x=水平, z=纵深, y=高度忽略)"""
    rooms = ROOM_DEFS.get(map_name, [])
    for (x1, x2, z1, z2), name in rooms:
        if x1 <= x <= x2 and z1 <= z <= z2:
            return name
    return "未知区域 (%.0f, %.0f)" % (x, z)


# -- 位置读取 (基于 eid/SimpleMoveComponent) --

def _parse_movement_args(data, pos):
    """
    解析 client_perform_movement 的 args 数据。
    格式: fixarray(5) [ fixarray(3)[x,y,z], fixarray(3)[dx,dy,dz], speed, ... ]
    返回 (x, y, z) 或 None
    """
    import struct
    if pos >= len(data):
        return None
    b = data[pos]
    # 期望 fixarray (0x90-0x9F)
    if not (0x90 <= b <= 0x9F):
        return None
    arr_len = b & 0x0F
    if arr_len < 1:
        return None
    # 第一个元素应该是 fixarray(3) = 位置
    pos += 1
    if pos >= len(data):
        return None
    b2 = data[pos]
    if b2 != 0x93:  # fixarray(3)
        return None
    pos += 1
    # 读取 3 个 float64 (CB + 8 bytes)
    coords = []
    for _ in range(3):
        if pos >= len(data) or data[pos] != 0xCB:
            return None
        if pos + 9 > len(data):
            return None
        val = struct.unpack(">d", data[pos + 1:pos + 9])[0]
        coords.append(val)
        pos += 9
    return tuple(coords)


def _read_player_positions(pm, pos_field_x=None, pos_field_y=None):
    """
    读取所有玩家的位置数据。
    游戏通过 SimpleMoveComponent 的 client_perform_movement 消息传输位置。
    args 格式: fixarray(5) [ fixarray(3)[x,y,z], ... ]
    eid (1000001~1000012) 对应玩家 1~12。
    返回 {copycat_index: (x, y, z)} 或 {}
    """
    regions = _get_readable_regions(pm.process_handle)
    CHUNK = 0x400000
    OVERLAP = 256
    # 搜索 "client_perform_movement" 后面跟 "args" 的模式
    func_key = _encode_msgpack_key(b'client_perform_movement')
    args_key = _encode_msgpack_key(b'args')
    eid_key = _encode_msgpack_key(b'eid')
    positions = {}  # ci -> (x, y, z)
    latest_addr = {}  # ci -> addr (保留最新的)

    for base, size in regions:
        for off in range(0, size, CHUNK):
            pre = min(OVERLAP, off)
            read_start = base + off - pre
            read_size = min(CHUNK + pre + OVERLAP, size - off + pre)
            if read_size <= 0:
                continue
            try:
                d = pm.read_bytes(read_start, read_size)
            except Exception:
                continue
            search_lo = pre
            search_hi = min(pre + CHUNK, len(d))
            pos = search_lo
            while True:
                idx = d.find(func_key, pos, search_hi)
                if idx == -1:
                    break
                # 读取 func_key 后面的值，应该是 "args" 字符串
                val_pos = idx + len(func_key)
                val_info = _read_msgpack_value(d, val_pos)
                if val_info and val_info[0] == "str" and val_info[1] == "args":
                    args_pos = val_pos + val_info[2]
                    coords = _parse_movement_args(d, args_pos)
                    if coords:
                        # 向前搜索 eid
                        search_back = d[max(0, idx - 128):idx]
                        eid_idx = search_back.rfind(eid_key)
                        if eid_idx >= 0:
                            eid_val_info = _read_msgpack_value(search_back, eid_idx + len(eid_key))
                            if eid_val_info and isinstance(eid_val_info[1], int):
                                eid_val = eid_val_info[1]
                                if 1000001 <= eid_val <= 1000012:
                                    ci = eid_val - 1000000
                                    abs_addr = read_start + idx
                                    # 保留地址最大的（最新的数据）
                                    if ci not in latest_addr or abs_addr > latest_addr[ci]:
                                        latest_addr[ci] = abs_addr
                                        positions[ci] = coords
                pos = idx + 1

    return positions


def _detect_map(pm, map_field=None):
    """
    尝试识别当前地图。
    优先搜索当前活跃对局的 scene_id (在 game_type 附近且无 end_game=true)。
    返回地图名称字符串。
    """
    if map_field:
        encoded = _encode_msgpack_key(map_field.encode("utf-8"))
        if not encoded:
            return "未知地图"
        regions = _get_readable_regions(pm.process_handle)
        for base, size in regions:
            try:
                d = pm.read_bytes(base, min(size, 0x400000))
            except Exception:
                continue
            idx = d.find(encoded)
            if idx >= 0:
                val_info = _read_msgpack_value(d, idx + len(encoded))
                if val_info:
                    if val_info[0] == "str":
                        return val_info[1]
                    elif val_info[0].startswith("int") or val_info[0].startswith("uint"):
                        return MAP_NAMES.get(val_info[1], "地图ID=%s" % val_info[1])
        return "未知地图"

    # 策略1: 在 game_type 附近找 scene_id (排除 end_game=true 的旧数据)
    regions = _get_readable_regions(pm.process_handle)
    CHUNK = 0x400000
    gt_key = _encode_msgpack_key(b'game_type')
    scene_key = _encode_msgpack_key(b'scene_id')
    eg_key = _encode_msgpack_key(b'end_game')

    for base, size in regions:
        for off in range(0, size, CHUNK):
            read_size = min(CHUNK, size - off)
            if read_size <= 0:
                continue
            try:
                d = pm.read_bytes(base + off, read_size)
            except Exception:
                continue
            pos = 0
            while True:
                idx = d.find(gt_key, pos)
                if idx == -1:
                    break
                # 检查 end_game
                lo = max(0, idx - 64)
                hi = min(len(d), idx + 400)
                eg_idx = d.find(eg_key, lo, hi)
                is_ended = False
                if eg_idx >= 0:
                    eg_val = _read_msgpack_value(d, eg_idx + len(eg_key))
                    if eg_val and eg_val[1] is True:
                        is_ended = True
                if not is_ended:
                    # 在附近找 scene_id
                    sc_idx = d.find(scene_key, lo, hi)
                    if sc_idx >= 0:
                        sc_val = _read_msgpack_value(d, sc_idx + len(scene_key))
                        if sc_val and isinstance(sc_val[1], int) and sc_val[1] in MAP_NAMES:
                            return MAP_NAMES[sc_val[1]]
                pos = idx + 1

    # 策略2: 搜索 scene_id 附近有 camera_active=True (当前活跃对局)
    cam_key = _encode_msgpack_key(b'camera_active')
    for base, size in regions:
        for off in range(0, size, CHUNK):
            read_size = min(CHUNK, size - off)
            if read_size <= 0:
                continue
            try:
                d = pm.read_bytes(base + off, read_size)
            except Exception:
                continue
            pos = 0
            while True:
                idx = d.find(scene_key, pos)
                if idx == -1:
                    break
                sc_val = _read_msgpack_value(d, idx + len(scene_key))
                if sc_val and isinstance(sc_val[1], int) and sc_val[1] in MAP_NAMES:
                    lo = max(0, idx - 128)
                    hi = min(len(d), idx + 128)
                    cam_idx = d.find(cam_key, lo, hi)
                    if cam_idx >= 0:
                        cam_val = _read_msgpack_value(d, cam_idx + len(cam_key))
                        if cam_val and cam_val[1] is True:
                            return MAP_NAMES[sc_val[1]]
                pos = idx + 1

    # 策略3: 取出现次数最少的已知 scene_id (当前对局副本少于旧对局)
    from collections import Counter
    map_counts = Counter()
    for base, size in regions:
        for off in range(0, size, CHUNK):
            read_size = min(CHUNK, size - off)
            if read_size <= 0:
                continue
            try:
                d = pm.read_bytes(base + off, read_size)
            except Exception:
                continue
            pos = 0
            while True:
                idx = d.find(scene_key, pos)
                if idx == -1:
                    break
                sc_val = _read_msgpack_value(d, idx + len(scene_key))
                if sc_val and isinstance(sc_val[1], int) and sc_val[1] in MAP_NAMES:
                    map_counts[sc_val[1]] += 1
                pos = idx + 1
    if map_counts:
        # 最少出现的已知地图最可能是当前对局
        least_common = map_counts.most_common()[-1]
        return MAP_NAMES[least_common[0]]

    # 策略4: 在玩家数据附近搜索
    entries = _scan_all_ci(pm, debug=False)
    pl = [e for e in entries if e[4]]
    if pl:
        addr = pl[0][3]
        try:
            block = pm.read_bytes(addr - 1024, 2048)
        except Exception:
            return "未知地图"
        keys = _extract_msgpack_keys(block, 0, len(block))
        for key_name, val_type, val, _ in keys:
            if "map" in key_name.lower() or key_name == "scene_id":
                if val_type == "str":
                    return val
                elif val_type.startswith("int") or val_type.startswith("uint"):
                    mapped = MAP_NAMES.get(val)
                    if mapped:
                        return mapped

    return "未知地图"


# -- 任务进度读取 --

def _read_task_progress(pm, task_field=None):
    """
    读取每个玩家的任务完成数。
    策略：
    1. 如果已知 task_field，直接读取该字段的整数值
    2. 否则在玩家数据附近搜索候选字段名
    3. 兜底：搜索所有整数字段，找值在 0~55 范围内且名称含 task/mission/quest 的
    返回 {copycat_index: task_count} 和 total_count
    """
    entries = _scan_all_ci(pm, debug=False)
    pl = [e for e in entries if e[4]]
    if not pl:
        return {}, 0

    # 每个 ci 取一个地址
    ci_addrs = {}
    for ci, camp, ident, addr, _ in pl:
        if ci not in ci_addrs:
            ci_addrs[ci] = addr

    task_counts = {}

    for ci in sorted(ci_addrs):
        addr = ci_addrs[ci]
        try:
            block = pm.read_bytes(addr - 512, 1024)
        except Exception:
            continue

        if task_field:
            # 已知字段名，直接搜索
            encoded = _encode_msgpack_key(task_field.encode("utf-8"))
            if encoded:
                idx = block.find(encoded)
                if idx >= 0:
                    val_info = _read_msgpack_value(block, idx + len(encoded))
                    if val_info and val_info[0] in ("int", "uint8", "uint16", "uint32",
                                                     "int8", "int16", "int32"):
                        task_counts[ci] = val_info[1]
                        continue

        # 未知字段名，搜索候选字段
        keys = _extract_msgpack_keys(block, 0, len(block))
        found = False
        for key_name, val_type, val, _ in keys:
            if val_type in ("int", "uint8", "uint16", "uint32", "int8", "int16", "int32"):
                kl = key_name.lower()
                if any(kw in kl for kw in ("task", "mission", "quest", "deduction",
                                            "finish", "complete", "done")):
                    if 0 <= val <= 55:
                        task_counts[ci] = val
                        found = True
                        break
        if not found:
            task_counts[ci] = 0

    total = sum(task_counts.values())
    return task_counts, total


def _display_task_progress(players, task_counts, total, self_ci=None, map_name=None):
    """显示任务进度表"""
    print()
    print("=" * 60)
    print("  演绎要求进度  (目标: %d)" % TASK_GOAL)
    if map_name and map_name != "未知地图":
        print("  地图: %s" % map_name)
    print("=" * 60)

    # 进度条
    bar_len = 30
    filled = int(bar_len * min(total, TASK_GOAL) / TASK_GOAL)
    bar = "█" * filled + "░" * (bar_len - filled)
    pct = min(100, total * 100 // TASK_GOAL) if TASK_GOAL > 0 else 0
    print("  总进度: [%s] %d/%d (%d%%)" % (bar, total, TASK_GOAL, pct))
    print()

    # 按任务数降序排列
    player_map = {ci: (camp, ident) for ci, camp, ident in players}
    sorted_players = sorted(player_map.keys(),
                            key=lambda c: task_counts.get(c, 0), reverse=True)

    print("  %-6s %-10s %-8s %-8s %s" % ("玩家", "阵营", "角色", "任务数", "贡献"))
    print("-" * 60)

    for ci in sorted_players:
        camp, ident = player_map[ci]
        camp_name = CAMP_NAMES.get(camp, "???")
        role_name = IDENTITY_MAP.get(ident, "?")
        icons = {1: "[侦探团]", 2: "[模仿者]", 3: "[神秘客]"}
        icon = icons.get(camp, "")
        count = task_counts.get(ci, 0)

        # 小进度条
        p_bar_len = 15
        p_filled = int(p_bar_len * min(count, 20) / 20) if count > 0 else 0
        p_bar = "▓" * p_filled + "░" * (p_bar_len - p_filled)

        marker = " ★" if ci == self_ci else ""
        print("  #%-4d %-10s %-8s %-4d     %s%s" % (
            ci, icon, role_name, count, p_bar, marker))

    print("=" * 60)

    # 阵营汇总
    camp_tasks = {}
    for ci in player_map:
        camp = player_map[ci][0]
        camp_tasks.setdefault(camp, 0)
        camp_tasks[camp] += task_counts.get(ci, 0)

    for camp_id in [1, 2, 3]:
        if camp_id in camp_tasks:
            label = CAMP_NAMES.get(camp_id, "???")
            print("  %s 合计: %d 个任务" % (label, camp_tasks[camp_id]))
    print()


def _task_tracker(pm, players, self_ci=None):
    """实时任务进度追踪模式"""
    import time
    try:
        import msvcrt
    except ImportError:
        msvcrt = None

    # 加载配置
    config_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")
    config = {}
    try:
        import json
        if os.path.exists(config_path):
            with open(config_path, "r", encoding="utf-8") as f:
                config = json.load(f)
    except Exception:
        pass

    task_field = config.get("task_field")
    map_field = config.get("map_field")

    if not players:
        print("[X] 没有玩家数据，请先扫描身份")
        return

    print("[i] 任务进度追踪模式 (按任意键退出)")
    print()

    while True:
        if msvcrt and msvcrt.kbhit():
            msvcrt.getch()
            break

        task_counts, total = _read_task_progress(pm, task_field)
        map_name = _detect_map(pm, map_field)

        os.system("cls" if os.name == "nt" else "clear")

        print("  第五人格 模仿者模式 - 任务进度追踪  %s" % time.strftime("%H:%M:%S"))
        _display_task_progress(players, task_counts, total, self_ci, map_name)
        print("  [按任意键退出]")

        time.sleep(1.0)

    print("\n[i] 已退出任务进度追踪模式")


# -- 投票监控 --

def _read_vote_data(pm):
    """
    搜索内存中的投票数据。
    投票通过 CopycatRoundDiscussSystem 传输。
    搜索 sync_vote / vote_target / on_vote 等函数调用。
    返回 {voter_ci: target_ci} 或 {}
    """
    regions = _get_readable_regions(pm.process_handle)
    CHUNK = 0x400000

    votes = {}  # voter -> target

    # 搜索投票相关的 msgpack 数据
    # 投票函数名候选
    vote_funcs = [b'sync_vote_client', b'on_vote_client', b'vote_client',
                  b'sync_vote', b'on_vote', b'do_vote']

    for func_name in vote_funcs:
        encoded = _encode_msgpack_key(func_name)
        if not encoded:
            continue
        for base, size in regions:
            for off in range(0, size, CHUNK):
                read_size = min(CHUNK, size - off)
                if read_size <= 0:
                    continue
                try:
                    d = pm.read_bytes(base + off, read_size)
                except Exception:
                    continue
                pos = 0
                while True:
                    idx = d.find(encoded, pos)
                    if idx == -1:
                        break
                    # 读取附近字段
                    local_start = max(0, idx - 64)
                    local_end = min(len(d), idx + 256)
                    keys = _extract_msgpack_keys(d, local_start, local_end)
                    voter = target = None
                    for key_name, val_type, val, _ in keys:
                        kl = key_name.lower()
                        if isinstance(val, int):
                            if "voter" in kl or "from" in kl or "src" in kl:
                                if 1 <= val <= 12:
                                    voter = val
                                elif 1000001 <= val <= 1000012:
                                    voter = val - 1000000
                            if "target" in kl or "to" in kl or "dst" in kl or "voted" in kl:
                                if 1 <= val <= 12:
                                    target = val
                                elif 1000001 <= val <= 1000012:
                                    target = val - 1000000
                        # eid 可能是投票者
                        if key_name == "eid" and isinstance(val, int):
                            if 1000001 <= val <= 1000012:
                                voter = val - 1000000
                    if voter and target:
                        votes[voter] = target
                    pos = idx + 1

    # 也搜索 CopycatRoundDiscussSystem 的投票同步消息
    # 搜索 "vote_idx" 或 "target_idx" 等字段
    for kw in [b'vote_idx', b'target_idx', b'vote_eid', b'target_eid',
               b'voter_idx', b'voted_idx']:
        encoded = _encode_msgpack_key(kw)
        if not encoded:
            continue
        for base, size in regions:
            for off in range(0, size, CHUNK):
                read_size = min(CHUNK, size - off)
                if read_size <= 0:
                    continue
                try:
                    d = pm.read_bytes(base + off, read_size)
                except Exception:
                    continue
                pos = 0
                while True:
                    idx = d.find(encoded, pos)
                    if idx == -1:
                        break
                    val_info = _read_msgpack_value(d, idx + len(encoded))
                    if val_info and isinstance(val_info[1], int):
                        print("  [发现] %s = %s @ 0x%X" % (
                            kw.decode(), val_info[1], base + off + idx))
                    pos = idx + 1

    return votes


def _display_votes(votes, players, self_ci=None):
    """显示投票情况"""
    if not votes:
        print("[i] 当前没有投票数据（可能不在会议中）")
        return

    player_map = {ci: (camp, ident) for ci, camp, ident in players}

    print()
    print("=" * 56)
    print("  投票情况")
    print("=" * 56)

    # 统计每个人被投了几票
    vote_counts = {}  # target -> count
    for voter, target in votes.items():
        vote_counts[target] = vote_counts.get(target, 0) + 1

    # 显示每个投票
    for voter in sorted(votes):
        target = votes[voter]
        v_name = IDENTITY_MAP.get(player_map.get(voter, (0, 0))[1], "?")
        t_name = IDENTITY_MAP.get(player_map.get(target, (0, 0))[1], "?")
        marker = " ★" if voter == self_ci else ""
        print("  #%-2d (%s) → 投票给 → #%-2d (%s)%s" % (
            voter, v_name, target, t_name, marker))

    print("-" * 56)

    # 被投票数排序
    sorted_targets = sorted(vote_counts.items(), key=lambda x: -x[1])
    for target, count in sorted_targets:
        t_name = IDENTITY_MAP.get(player_map.get(target, (0, 0))[1], "?")
        camp = player_map.get(target, (0, 0))[0]
        camp_icon = {1: "[侦探团]", 2: "[模仿者]", 3: "[神秘客]"}.get(camp, "")
        bar = "▓" * count
        print("  #%-2d %s %-8s: %d票 %s" % (target, camp_icon, t_name, count, bar))

    print("=" * 56)
    print()


# -- 实时位置追踪 --

def _position_tracker(pm, players, self_ci=None):
    """实时位置追踪模式，每秒刷新"""
    import time
    try:
        import msvcrt
    except ImportError:
        msvcrt = None

    # 加载配置
    config_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")
    config = {}
    try:
        import json
        if os.path.exists(config_path):
            with open(config_path, "r", encoding="utf-8") as f:
                config = json.load(f)
    except Exception:
        pass

    pos_x = config.get("pos_field_x")
    pos_y = config.get("pos_field_y")
    map_field = config.get("map_field")
    task_field = config.get("task_field")

    if not players:
        print("[X] 没有玩家数据，请先扫描身份")
        return

    player_map = {ci: (camp, ident) for ci, camp, ident in players}

    print("[i] 位置追踪模式 (按任意键退出)")
    print("[i] 位置来源: eid/SimpleMoveComponent (pos_x, pos_y, pos_z)")
    print()

    while True:
        # 检测按键退出
        if msvcrt and msvcrt.kbhit():
            msvcrt.getch()
            break

        positions = _read_player_positions(pm)
        map_name = _detect_map(pm, map_field)
        task_counts, task_total = _read_task_progress(pm, task_field)

        # 清屏
        os.system("cls" if os.name == "nt" else "clear")

        print("=" * 80)
        print("  第五人格 模仿者模式 - 实时位置追踪")
        print("  地图: %s    %s" % (map_name, time.strftime("%H:%M:%S")))

        # 任务进度条
        bar_len = 25
        filled = int(bar_len * min(task_total, TASK_GOAL) / TASK_GOAL)
        bar = "█" * filled + "░" * (bar_len - filled)
        pct = min(100, task_total * 100 // TASK_GOAL) if TASK_GOAL > 0 else 0
        print("  演绎进度: [%s] %d/%d (%d%%)" % (bar, task_total, TASK_GOAL, pct))
        print("=" * 80)

        # 显示自己
        if self_ci and self_ci in player_map:
            camp, ident = player_map[self_ci]
            camp_name = CAMP_NAMES.get(camp, "???")
            role_name = IDENTITY_MAP.get(ident, "?")
            icons = {1: "[侦探团]", 2: "[模仿者]", 3: "[神秘客]"}
            pos = positions.get(self_ci)
            loc = ""
            if pos:
                loc = _coord_to_room(map_name, pos[0], pos[2])
            tc = task_counts.get(self_ci, 0)
            print("  ★ 你 = 玩家#%d  %s  %s  %s  任务:%d  %s" % (
                self_ci, icons.get(camp, ""), camp_name, role_name, tc, loc))
            print("-" * 80)

        # 表头
        print("  %-6s %-10s %-8s %-6s %-8s %-8s %s" % (
            "玩家", "阵营", "角色", "任务", "X", "Z", "位置"))
        print("-" * 80)

        for ci in sorted(player_map):
            camp, ident = player_map[ci]
            role_name = IDENTITY_MAP.get(ident, "?")
            icons = {1: "[侦探团]", 2: "[模仿者]", 3: "[神秘客]"}
            icon = icons.get(camp, "")
            tc = task_counts.get(ci, 0)

            pos = positions.get(ci)
            if pos:
                x_str = "%.1f" % pos[0]
                z_str = "%.1f" % pos[2]
                room = _coord_to_room(map_name, pos[0], pos[2])
            else:
                x_str = "-"
                z_str = "-"
                room = "未知"

            marker = " ★" if ci == self_ci else ""
            print("  #%-4d %-10s %-8s %-6d %-8s %-8s %s%s" % (
                ci, icon, role_name, tc, x_str, z_str, room, marker))

        print("=" * 80)
        print("  [按任意键退出]")

        time.sleep(1.0)

    print("\n[i] 已退出位置追踪模式")


# -- 校准命令 --

def _calibrate(pm, players):
    """校准位置字段名和房间坐标"""
    import json
    config_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")

    print("\n[校准] 先发现可用字段...")
    all_keys = _discover_fields(pm)
    if not all_keys:
        return

    # 找出 float 字段
    float_fields = [k for k, v in all_keys.items()
                    if any(t.startswith("float") for t, _, _ in v)]

    if float_fields:
        print("\n[i] 发现的 float 字段 (可能是坐标):")
        for i, name in enumerate(float_fields):
            vals = all_keys[name]
            sample = ", ".join("%.2f" % v for _, v, _ in vals[:3])
            print("  [%d] %s = %s" % (i, name, sample))

        try:
            x_choice = input("\n选择 X 坐标字段编号: ").strip()
            y_choice = input("选择 Y 坐标字段编号: ").strip()
            x_field = float_fields[int(x_choice)]
            y_field = float_fields[int(y_choice)]
        except (ValueError, IndexError, EOFError, KeyboardInterrupt):
            print("[X] 取消")
            return

        # 找 map 字段
        map_fields = [k for k in all_keys if "map" in k.lower()]
        map_field = None
        if map_fields:
            print("\n[i] 发现的地图相关字段:")
            for i, name in enumerate(map_fields):
                vals = all_keys[name]
                sample = ", ".join(str(v) for _, v, _ in vals[:3])
                print("  [%d] %s = %s" % (i, name, sample))
            try:
                m_choice = input("选择地图字段编号 (回车跳过): ").strip()
                if m_choice:
                    map_field = map_fields[int(m_choice)]
            except (ValueError, IndexError):
                pass

        # 保存
        config = {}
        try:
            if os.path.exists(config_path):
                with open(config_path, "r", encoding="utf-8") as f:
                    config = json.load(f)
        except Exception:
            pass

        config["pos_field_x"] = x_field
        config["pos_field_y"] = y_field
        if map_field:
            config["map_field"] = map_field

        # 找 task 字段
        int_fields = [k for k, v in all_keys.items()
                      if any(t in ("int", "uint8", "uint16", "uint32", "int8", "int16", "int32")
                             for t, _, _ in v)]
        task_candidates = [k for k in int_fields
                           if any(kw in k.lower() for kw in
                                  ("task", "mission", "quest", "deduction",
                                   "finish", "complete", "done", "count"))]
        if task_candidates:
            print("\n[i] 可能的任务进度字段:")
            for i, name in enumerate(task_candidates):
                vals = all_keys[name]
                sample = ", ".join(str(v) for _, v, _ in vals[:3])
                print("  [%d] %s = %s" % (i, name, sample))
            try:
                t_choice = input("选择任务进度字段编号 (回车跳过): ").strip()
                if t_choice:
                    config["task_field"] = task_candidates[int(t_choice)]
            except (ValueError, IndexError):
                pass

        with open(config_path, "w", encoding="utf-8") as f:
            json.dump(config, f, indent=2, ensure_ascii=False)
        print("[OK] 已保存: x=%s, y=%s, map=%s, task=%s" % (
            x_field, y_field, map_field or "自动", config.get("task_field", "自动")))
    else:
        print("[X] 未发现 float 字段，可能不在对局中")


# -- 普通模式 (1v4) 扫描 --

# 监管者角色 ID 映射
HUNTER_MAP = {
    1: "厂长", 2: "鹿头", 3: "小丑", 4: "杰克", 5: "红蝶",
    6: "黄衣之主", 7: "宿伞之魂", 8: "疯眼", 9: "梦之女巫",
    10: "摄影师", 11: "蜥蜴人", 12: "使徒", 13: "破轮",
    14: "邦邦", 15: "约瑟夫", 16: "贞子", 17: "噩梦",
    18: "26号守卫", 19: "爱哭鬼", 20: "血之女王",
    21: "寄居蟹", 22: "白无常", 23: "黑无常",
}

# 求生者角色 ID 映射
SURVIVOR_MAP = {
    1: "医生", 2: "律师", 3: "冒险家", 4: "慈善家",
    5: "园丁", 6: "魔术师", 7: "机械师", 8: "前锋",
    9: "空军", 10: "佣兵", 11: "调香师", 12: "祭司",
    13: "盲女", 14: "先知", 15: "舞女", 16: "杂技演员",
    17: "入殓师", 18: "勘探员", 19: "咒术师", 20: "野人",
    21: "邮差", 22: "画家", 23: "幸运儿", 24: "心理学家",
    25: "囚徒", 26: "昆虫学者", 27: "患者", 28: "调酒师",
}

# 普通模式地图 (scene_id -> 名称)
NORMAL_MAP_NAMES = {
    2: "军工厂", 3: "圣心医院", 5: "红教堂", 8: "月亮河公园",
    9: "湖景村", 10: "永眠镇", 11: "唐人街", 12: "里奥的回忆",
    13: "白沙街疯人院", 14: "闪金石窟",
    # scene_id 可能是其他值，需要实际确认
}


# 技能 ID → 角色名映射
# 监管者技能在 700-999 范围，求生者技能在 3000-4999 范围
# 每个角色有一个唯一的技能基础 ID，通过游戏内数据逐步补充
SKILL_TO_HUNTER = {
    # skill_id: 角色名 (通过实际游戏数据确认)
    711: "红夫人",
    # 以下为推测，需要实际验证:
    # 701: "厂长", 702: "小丑", 703: "鹿头", 704: "杰克",
    # 705: "蜘蛛", 706: "红蝶", 707: "黄衣之主", 708: "宿伞之魂",
    # 709: "摄影师", 710: "疯眼",
}

SKILL_TO_SURVIVOR = {
    # skill_id: 角色名 (通过实际游戏数据确认)
    3003: "前锋",
    # 以下为推测，需要实际验证:
    # 每个求生者有 4 个技能 (base, base+1, base+2, base+3)
}


def _skill_to_character(skill_id, is_hunter=False):
    """根据技能 ID 查找角色名。
    先查精确匹配，再查范围匹配 (同一角色的多个技能变体)。
    """
    if is_hunter:
        if skill_id in SKILL_TO_HUNTER:
            return SKILL_TO_HUNTER[skill_id]
        # 尝试查找附近的 skill_id (同角色可能有多个技能)
        for base_skill, name in SKILL_TO_HUNTER.items():
            if abs(skill_id - base_skill) <= 5:
                return name
    else:
        if skill_id in SKILL_TO_SURVIVOR:
            return SKILL_TO_SURVIVOR[skill_id]
        for base_skill, name in SKILL_TO_SURVIVOR.items():
            if abs(skill_id - base_skill) <= 5:
                return name
    return ""


def _find_current_game_eids(pm):
    """
    找到当前活跃对局的玩家 eid 列表。
    策略: 搜索 et+uid 实体速度数据，按内存地址聚类找到当前对局的实体。
    返回 (player_eids: set, entities: dict, self_eid: int or None)
      entities = {uid: {"unit_type": int, "is_bot": bool, "name": str}}
      self_eid = 非 bot 的玩家 uid (暂时无法确定，返回 None)
    """
    regions = _get_readable_regions(pm.process_handle)
    CHUNK = 0x400000
    et_key = _encode_msgpack_key(b'et')
    uid_key = _encode_msgpack_key(b'uid')

    # 第一步: 收集所有 et+uid 条目
    entries = []  # [(uid, addr)]
    for base, size in regions:
        for off in range(0, size, CHUNK):
            read_size = min(CHUNK, size - off)
            if read_size <= 0:
                continue
            try:
                d = pm.read_bytes(base + off, read_size)
            except Exception:
                continue
            pos = 0
            while True:
                idx = d.find(et_key, pos)
                if idx == -1:
                    break
                et_val = _read_msgpack_value(d, idx + len(et_key))
                if et_val and isinstance(et_val[1], int) and et_val[1] == 7:
                    after_pos = idx + len(et_key) + et_val[2]
                    if after_pos + 20 < len(d):
                        uid_idx = d.find(uid_key, after_pos, after_pos + 20)
                        if uid_idx >= 0:
                            uid_val = _read_msgpack_value(d, uid_idx + len(uid_key))
                            if uid_val and isinstance(uid_val[1], int):
                                v = uid_val[1]
                                if 1000000 <= v <= 1100000:
                                    abs_addr = base + off + idx
                                    entries.append((v, abs_addr))
                pos = idx + 1

    if not entries:
        return set(), {}, None

    # 第二步: 按地址聚类 (间距 < 64KB 的归为一组)
    entries.sort(key=lambda x: x[1])
    clusters = []
    current_cluster = [entries[0]]
    for i in range(1, len(entries)):
        if entries[i][1] - entries[i - 1][1] < 65536:
            current_cluster.append(entries[i])
        else:
            clusters.append(current_cluster)
            current_cluster = [entries[i]]
    clusters.append(current_cluster)

    # 第三步: 选最大的聚类 (如果相同大小，选地址最高的)
    best = max(clusters, key=lambda c: (len(c), max(a for _, a in c)))
    player_eids = set()
    for uid, addr in best:
        player_eids.add(uid)

    # 如果聚类只有部分玩家，也把散落的同 uid 加入
    # (有些实体可能在不同内存块)
    all_uids_in_entries = set(uid for uid, _ in entries)
    # 只保留在最大聚类中出现的 uid，加上与聚类 uid 范围接近的
    if player_eids:
        min_uid = min(player_eids)
        max_uid = max(player_eids)
        for uid in all_uids_in_entries:
            if min_uid <= uid <= max_uid + 10:
                player_eids.add(uid)

    # 第四步: 识别玩家身份
    # 策略1: self_uid → 找到"我"
    # 策略2: skill_id → 监管者技能在 700+ 范围，求生者技能在 3000+ 范围
    # 策略3: is_bot → 区分真人和 AI
    entities = {}
    for uid in player_eids:
        entities[uid] = {
            "unit_type": 2,
            "is_bot": True,
            "name": "",
            "character": "",
            "character_id": 0,
        }

    self_uid_key = _encode_msgpack_key(b'self_uid')
    skill_key = _encode_msgpack_key(b'skill_id')
    uid_key2 = _encode_msgpack_key(b'uid')
    self_eid = None

    # 4a: 找 self_uid (出现最多的值 = 玩家自己)
    from collections import Counter
    self_uid_counts = Counter()
    for base, size in regions:
        for off in range(0, size, CHUNK):
            read_size = min(CHUNK, size - off)
            if read_size <= 0:
                continue
            try:
                d = pm.read_bytes(base + off, read_size)
            except Exception:
                continue
            pos = 0
            while True:
                idx = d.find(self_uid_key, pos)
                if idx == -1:
                    break
                val = _read_msgpack_value(d, idx + len(self_uid_key))
                if val and isinstance(val[1], int) and val[1] in player_eids:
                    self_uid_counts[val[1]] += 1
                pos = idx + 1

    if self_uid_counts:
        self_eid = self_uid_counts.most_common(1)[0][0]
        entities[self_eid]["is_bot"] = False

    # 4b: 收集每个 uid 的 skill_id 集合
    uid_skills = {}  # uid -> set of skill_ids
    for base, size in regions:
        for off in range(0, size, CHUNK):
            read_size = min(CHUNK, size - off)
            if read_size <= 0:
                continue
            try:
                d = pm.read_bytes(base + off, read_size)
            except Exception:
                continue
            pos = 0
            while True:
                idx = d.find(skill_key, pos)
                if idx == -1:
                    break
                val = _read_msgpack_value(d, idx + len(skill_key))
                if val and isinstance(val[1], int) and val[1] > 0:
                    lo = max(0, idx - 200)
                    hi = min(len(d), idx + 200)
                    block = d[lo:hi]
                    uid_idx = block.rfind(uid_key2, 0, idx - lo + 10)
                    if uid_idx < 0:
                        uid_idx = block.find(uid_key2)
                    if uid_idx >= 0:
                        uv = _read_msgpack_value(block, uid_idx + len(uid_key2))
                        if uv and isinstance(uv[1], int) and uv[1] in player_eids:
                            uid_skills.setdefault(uv[1], set()).add(val[1])
                pos = idx + 1

    # 4c: 根据 skill_id 判断监管者/求生者 + 识别角色
    # 监管者技能: 700-999 范围 (如红夫人 711)
    # 求生者技能: 3000+ 范围 (如前锋 3003-3006)
    # 通用技能: 9, 11, 2601, 2611 等 (所有角色都有)
    hunter_uid = None
    for uid, skills in uid_skills.items():
        hunter_skills = [s for s in skills if 700 <= s <= 999]
        if hunter_skills:
            hunter_uid = uid
            entities[uid]["unit_type"] = 1
            # 识别监管者角色: 用最小的 700+ 技能 ID
            h_skill = min(hunter_skills)
            char_name = _skill_to_character(h_skill, is_hunter=True)
            if char_name:
                entities[uid]["character"] = char_name
            break

    # 如果没找到 700-999 范围的技能，找技能最多且不是自己的
    if not hunter_uid and uid_skills:
        candidates = [(uid, len(skills)) for uid, skills in uid_skills.items()
                       if uid != self_eid and len(skills) > 3]
        if candidates:
            candidates.sort(key=lambda x: -x[1])
            hunter_uid = candidates[0][0]
            entities[hunter_uid]["unit_type"] = 1

    # 识别求生者角色: 用 3000+ 范围的最小技能 ID
    for uid, skills in uid_skills.items():
        if uid == hunter_uid:
            continue
        surv_skills = [s for s in skills if 3000 <= s <= 4999]
        if surv_skills:
            s_skill = min(surv_skills)
            char_name = _skill_to_character(s_skill, is_hunter=False)
            if char_name:
                entities[uid]["character"] = char_name

    # 4d: 判断 bot vs 真人
    # 策略: 有多个技能 (>1) 的是真人角色，只有基础技能 (skill 9) 的是 bot
    # self_uid 已确认为真人
    for uid, skills in uid_skills.items():
        # 排除通用技能 (9, 11, 2601-2612)
        unique_skills = [s for s in skills if s not in (9, 11) and not (2601 <= s <= 2620)]
        if len(unique_skills) > 0:
            entities[uid]["is_bot"] = False

    # 确保 self_eid 标记为非 bot
    if self_eid:
        entities[self_eid]["is_bot"] = False

    # 打印技能摘要 (帮助发现新的 skill→角色 映射)
    if uid_skills:
        for uid in sorted(uid_skills.keys()):
            skills = sorted(uid_skills[uid])
            char = entities.get(uid, {}).get("character", "")
            ut = entities.get(uid, {}).get("unit_type", 2)
            role_tag = "H" if ut == 1 else "S"
            # 检查是否有未映射的技能
            if not char:
                unique = [s for s in skills if s not in (9, 11) and not (2601 <= s <= 2620)]
                if unique:
                    print("[skill] uid=%d [%s] 未识别角色, 技能=%s" % (uid, role_tag, unique))

    return player_eids, entities, self_eid


# 位置扫描缓存: 记住哪些内存区域包含位置数据，避免每次全扫描
_pos_cached_regions = []   # [(base, size)] 上次找到位置数据的区域
_pos_cache_ts = 0          # 上次全扫描时间
_POS_CACHE_TTL = 5.0       # 缓存有效期 (秒)，过期后重新全扫描


def _scan_positions_in_regions(pm, scan_regions, filter_eids=None):
    """在指定的内存区域中扫描位置数据"""
    CHUNK = 0x400000
    OVERLAP = 256
    func_key = _encode_msgpack_key(b'client_perform_movement')
    eid_key = _encode_msgpack_key(b'eid')
    positions = {}
    latest_addr = {}
    hit_regions = set()

    for base, size in scan_regions:
        for off in range(0, size, CHUNK):
            pre = min(OVERLAP, off)
            read_start = base + off - pre
            read_size = min(CHUNK + pre + OVERLAP, size - off + pre)
            if read_size <= 0:
                continue
            try:
                d = pm.read_bytes(read_start, read_size)
            except Exception:
                continue
            search_lo = pre
            search_hi = min(pre + CHUNK, len(d))
            pos = search_lo
            while True:
                idx = d.find(func_key, pos, search_hi)
                if idx == -1:
                    break
                val_pos = idx + len(func_key)
                val_info = _read_msgpack_value(d, val_pos)
                if val_info and val_info[0] == "str" and val_info[1] == "args":
                    args_pos = val_pos + val_info[2]
                    coords = _parse_movement_args(d, args_pos)
                    if coords:
                        search_back = d[max(0, idx - 128):idx]
                        eid_idx = search_back.rfind(eid_key)
                        if eid_idx >= 0:
                            eid_val_info = _read_msgpack_value(search_back, eid_idx + len(eid_key))
                            if eid_val_info and isinstance(eid_val_info[1], int):
                                eid_val = eid_val_info[1]
                                if filter_eids and eid_val not in filter_eids:
                                    pos = idx + 1
                                    continue
                                abs_addr = read_start + idx
                                if eid_val not in latest_addr or abs_addr > latest_addr[eid_val]:
                                    latest_addr[eid_val] = abs_addr
                                    positions[eid_val] = coords
                                    hit_regions.add((base, size))
                pos = idx + 1

    return positions, latest_addr, list(hit_regions)


def _read_all_positions(pm, filter_eids=None, max_players=None):
    """
    读取所有 eid 的位置。使用缓存加速: 先扫描上次有数据的区域，
    找不到足够数据时才全扫描。
    返回 {eid: (x, y, z)}
    """
    global _pos_cached_regions, _pos_cache_ts

    now = time.time()
    need_full = (now - _pos_cache_ts > _POS_CACHE_TTL) or not _pos_cached_regions

    # 先尝试缓存区域 (快速)
    if _pos_cached_regions and not need_full:
        positions, latest_addr, hit = _scan_positions_in_regions(
            pm, _pos_cached_regions, filter_eids)
        if positions:
            if max_players and len(positions) > max_players:
                sorted_eids = sorted(latest_addr.keys(), key=lambda e: latest_addr[e], reverse=True)
                top_eids = set(sorted_eids[:max_players])
                positions = {e: p for e, p in positions.items() if e in top_eids}
            return positions

    # 全扫描
    all_regions = _get_readable_regions(pm.process_handle)
    positions, latest_addr, hit = _scan_positions_in_regions(
        pm, all_regions, filter_eids)

    # 更新缓存: 记住有数据的区域，下次优先扫描
    if hit:
        # 扩展缓存区域 (前后各扩展 1MB，因为数据可能移动)
        expanded = set()
        for base, size in hit:
            exp_base = max(0, base - 0x100000)
            exp_size = size + 0x200000
            expanded.add((exp_base, exp_size))
        _pos_cached_regions = list(expanded)
    _pos_cache_ts = now

    if max_players and len(positions) > max_players:
        sorted_eids = sorted(latest_addr.keys(), key=lambda e: latest_addr[e], reverse=True)
        top_eids = set(sorted_eids[:max_players])
        positions = {e: p for e, p in positions.items() if e in top_eids}

    return positions


def _detect_game_mode(pm):
    """
    自动检测当前游戏模式。
    通过 game_type 字段判断:
      game_type=20 + match_type=73 → 模仿者模式
      game_type=7 → 普通/测试模式
    排除 end_game=true 的已结束对局数据。
    返回 "copycat" / "normal" / "unknown"
    """
    regions = _get_readable_regions(pm.process_handle)
    CHUNK = 0x400000
    gt_key = _encode_msgpack_key(b'game_type')
    from collections import Counter
    active_types = Counter()

    for base, size in regions:
        for off in range(0, size, CHUNK):
            read_size = min(CHUNK, size - off)
            if read_size <= 0:
                continue
            try:
                d = pm.read_bytes(base + off, read_size)
            except Exception:
                continue
            pos = 0
            while True:
                idx = d.find(gt_key, pos)
                if idx == -1:
                    break
                val_info = _read_msgpack_value(d, idx + len(gt_key))
                if val_info and isinstance(val_info[1], int):
                    gt_val = val_info[1]
                    # 检查附近有没有 end_game=true (已结束的对局)
                    lo = max(0, idx - 64)
                    hi = min(len(d), idx + 256)
                    eg_key = _encode_msgpack_key(b'end_game')
                    eg_idx = d.find(eg_key, lo, hi)
                    is_ended = False
                    if eg_idx >= 0:
                        eg_val = _read_msgpack_value(d, eg_idx + len(eg_key))
                        if eg_val and eg_val[1] is True:
                            is_ended = True
                    if not is_ended:
                        active_types[gt_val] += 1
                pos = idx + 1

    # game_type=20 且没有 end_game=true → 模仿者模式进行中
    # game_type=7 → 普通/测试模式
    # 用出现次数最多的 game_type 判断 (避免被残留数据干扰)
    if active_types:
        most_common_gt = active_types.most_common(1)[0][0]
        if most_common_gt == 20:
            return "copycat"
        elif most_common_gt in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10):
            return "normal"
        # 如果最多的不是已知类型，看看有没有 20
        if 20 in active_types and active_types[20] >= 3:
            return "copycat"
        # 看看有没有普通模式类型
        for gt_val in active_types:
            if gt_val in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10):
                return "normal"

    # 兜底：通过 eid 数量判断
    positions = _read_all_positions(pm)
    if positions:
        player_eids = [e for e in positions if 1000001 <= e <= 1000012]
        if len(player_eids) >= 8:
            return "copycat"
        elif len(positions) <= 7:
            return "normal"
        else:
            return "copycat"

    return "unknown"


def _scan_normal_mode_players(pm):
    """
    普通模式下扫描玩家信息。
    通过 entities 列表获取 uid/unit_type/is_bot/name/character。
    返回 {eid: {"role": str, "camp": int, "ident": int, "name": str, "character": str}}
    camp: 1=求生者, 2=监管者
    """
    player_eids, entities, self_eid = _find_current_game_eids(pm)
    if not player_eids:
        # 兜底
        positions = _read_all_positions(pm, max_players=5)
        players = {}
        for eid in positions:
            players[eid] = {"role": "?", "camp": 1, "ident": 0, "name": "", "character": ""}
        return players

    players = {}
    for uid, info in entities.items():
        ut = info.get("unit_type")
        player_name = info.get("name", "")
        character = info.get("character", "")
        char_id = info.get("character_id", 0)
        is_bot = info.get("is_bot", True)

        if ut == 1:
            camp = 2  # 监管者
            if not character:
                character = HUNTER_NAMES.get(char_id, "监管者")
        else:
            camp = 1  # 求生者
            if not character:
                character = SURVIVOR_NAMES.get(char_id, "求生者")

        role = character
        if player_name:
            role = player_name

        players[uid] = {
            "role": role,
            "camp": camp,
            "ident": char_id,
            "name": player_name,
            "character": character,
            "is_bot": is_bot,
        }

    return players


# -- 网页地图模式 --

def _web_map_mode(pm, mode, players=None, self_ci=None):
    """启动网页地图模式"""
    try:
        from web_server import GameState, start_server, update_loop
        from qte import QTEAutoCalibrator
    except ImportError as e:
        print("[X] 缺少模块: %s" % e)
        return

    state = GameState()
    state.mode = mode

    # 设置玩家数据
    if players:
        player_dict = {}
        for ci, camp, ident in players:
            role_name = IDENTITY_MAP.get(ident, "?")
            camp_label = CAMP_NAMES.get(camp, "?")
            player_dict[str(ci)] = {
                "name": "",
                "camp": camp,
                "role": role_name,
                "character": role_name,
                "ident": ident,
                "is_bot": None,
            }
        state.players = player_dict

    if self_ci:
        state.self_id = self_ci

    # 启动 QTE 自动校准器
    qte = QTEAutoCalibrator(pm, sys.modules[__name__], state)

    # 启动 web server
    server, thread = start_server(state, port=8888)

    print("[i] 网页地图已启动，浏览器应自动打开")
    print("[i] 按 q 退出, t 切换 QTE")

    # 进入更新循环
    import reader_plain as rp
    update_loop(pm, state, rp)

    server.shutdown()
    qte.stop()


# -- 主程序 --

def main():
    print("=" * 56)
    print("  第五人格 多功能工具 v3")
    print("=" * 56)
    print()

    try:
        pm = pymem.Pymem(PROCESS_NAME)
        print("[OK] 已连接 %s (PID: %d)" % (PROCESS_NAME, pm.process_id))
    except pymem.exception.ProcessNotFound:
        print("[X] 未找到 %s，请先启动游戏" % PROCESS_NAME)
        sys.exit(1)
    except pymem.exception.CouldNotOpenProcess:
        print("[X] 权限不足，请以管理员身份运行")
        sys.exit(1)

    # 自动检测游戏模式
    print("[...] 检测游戏模式...")
    game_mode = _detect_game_mode(pm)
    mode_names = {"copycat": "模仿者模式", "normal": "普通模式", "unknown": "未知"}
    print("[i] 检测到: %s" % mode_names.get(game_mode, "未知"))

    if game_mode == "unknown":
        print("[i] 未检测到对局数据，可能不在游戏中")
        print("[i] 手动选择: 1=模仿者模式  2=普通模式  w=直接启动网页地图")
        try:
            choice = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            return
        if choice == "1":
            game_mode = "copycat"
        elif choice == "2":
            game_mode = "normal"
        elif choice == "w":
            game_mode = "normal"
            _web_map_mode(pm, game_mode)
            return
        else:
            return

    # 普通模式直接进入网页地图
    if game_mode == "normal":
        print()
        print("[i] 普通模式 — 自动启动网页地图")
        map_name = _detect_map(pm)
        print("[i] 地图: %s" % map_name)
        # 找当前对局的实体
        player_eids, entities, self_eid = _find_current_game_eids(pm)
        if player_eids:
            print("[i] 当前对局玩家:")
            for uid, info in sorted(entities.items()):
                ut = info.get("unit_type")
                bot = info.get("is_bot")
                name = info.get("name", "")
                character = info.get("character", "")
                camp = "监管者" if ut == 1 else "求生者"
                marker = " ★你" if bot is False else " (bot)"
                name_str = name if name else "?"
                char_str = " [%s]" % character if character else ""
                print("    uid=%d  %s%s  %s%s" % (uid, camp, char_str, name_str, marker))
            positions = _read_all_positions(pm, filter_eids=player_eids)
        else:
            print("[i] 未找到实体列表，使用最新 5 个位置")
            positions = _read_all_positions(pm, max_players=5)
        print("[i] 找到 %d 个玩家位置" % len(positions))
        if self_eid:
            print("[i] 你的 eid: %d" % self_eid)
        _web_map_mode(pm, game_mode)
        return

    # ── 以下是模仿者模式 ──

    # 尝试识别自己
    self_ci = None
    self_ci = _detect_self(pm)
    if self_ci:
        print("[i] 检测到你是玩家 #%d" % self_ci)
    else:
        print("[i] 未能自动识别你的身份，可用 'me' 命令手动设置")

    print()
    print("[命令列表]")
    print("  回车  = 刷新身份    n = 新一局    d = 调试模式")
    print("  w     = 网页地图    p = 位置追踪  t = 任务进度")
    print("  v     = 投票监控    b = 移除黑灯  me = 设置自己")
    print("  f     = 发现字段    cal = 校准    x = 原始dump")
    print("  qte   = QTE字段发现  q = 退出")
    print()

    debug = False
    prev_result = None
    players = []

    while True:
        print("[...] 扫描内存...")
        players = read_identities(pm, debug=debug, prev_result=prev_result)

        if players:
            display(players)
            prev_result = players
            # 显示自己的身份
            if self_ci:
                for ci, camp, ident in players:
                    if ci == self_ci:
                        camp_name = CAMP_NAMES.get(camp, "???")
                        role_name = IDENTITY_MAP.get(ident, "?")
                        print("  ★ 你是玩家 #%d  %s  %s" % (ci, camp_name, role_name))
                        break
            # 显示任务进度概要
            task_counts, task_total = _read_task_progress(pm)
            if task_total > 0 or any(v > 0 for v in task_counts.values()):
                bar_len = 25
                filled = int(bar_len * min(task_total, TASK_GOAL) / TASK_GOAL)
                bar = "█" * filled + "░" * (bar_len - filled)
                pct = min(100, task_total * 100 // TASK_GOAL) if TASK_GOAL > 0 else 0
                print("  演绎进度: [%s] %d/%d (%d%%)" % (bar, task_total, TASK_GOAL, pct))
                # 显示做了任务的玩家
                active = [(ci, c) for ci, c in task_counts.items() if c > 0]
                if active:
                    active.sort(key=lambda x: -x[1])
                    parts = ["#%d=%d" % (ci, c) for ci, c in active]
                    print("  任务分布: %s" % ", ".join(parts))
                print()
        else:
            print("[X] 未找到玩家数据，确认是否在模仿者对局中")

        try:
            cmd = input("> ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            break

        if cmd == "q":
            print("Bye")
            break
        elif cmd == "d":
            debug = not debug
            print("[i] 调试模式: %s" % ("开" if debug else "关"))
        elif cmd == "n":
            prev_result = players
            print("[i] 已标记新一局，下次扫描将优先选择新数据")
        elif cmd == "x":
            _dump_raw_matches(pm)
        elif cmd == "r":
            _dump_format_a_region(pm)
        elif cmd == "f":
            _discover_fields(pm)
        elif cmd == "b":
            _smart_blackout(pm, players, self_ci)
        elif cmd == "p":
            _position_tracker(pm, players, self_ci)
        elif cmd == "t":
            task_counts, task_total = _read_task_progress(pm)
            map_name = _detect_map(pm)
            _display_task_progress(players, task_counts, task_total, self_ci, map_name)
            try:
                sub = input("  输入 'r' 进入实时追踪, 回车返回: ").strip().lower()
            except (EOFError, KeyboardInterrupt):
                sub = ""
            if sub == "r":
                _task_tracker(pm, players, self_ci)
        elif cmd == "cal":
            _calibrate(pm, players)
        elif cmd == "v":
            votes = _read_vote_data(pm)
            _display_votes(votes, players, self_ci)
        elif cmd == "me":
            try:
                num = input("输入你的玩家编号 (1-12): ").strip()
                self_ci = int(num)
                print("[OK] 已设置你为玩家 #%d" % self_ci)
            except (ValueError, EOFError, KeyboardInterrupt):
                print("[X] 无效输入")
        elif cmd == "w":
            _web_map_mode(pm, game_mode, players, self_ci)
        elif cmd == "qte":
            try:
                from qte import discover_qte_fields
                discover_qte_fields(pm, sys.modules[__name__])
            except ImportError:
                print("[X] 缺少 qte.py 模块")


if __name__ == "__main__":
    main()
