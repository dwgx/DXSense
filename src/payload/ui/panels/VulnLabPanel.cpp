#include "VulnLabPanel.hpp"

#include "core/Config.hpp"
#include "game/CameraSampler.hpp"
#include "scripting/PythonBridge.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"
#include "utils/JsonLite.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

namespace dxs {

namespace {

constexpr double k_install_retry_sec = 3.0;
constexpr std::array<int, 6> k_cfg_sids{199, 10, 137, 127, 8, 13};
constexpr std::array<const char*, 6> k_cfg_labels{
    "199 watcher", "10 hunter", "137 no-hit", "127 survivor", "8 survivor", "13 survivor",
};

constexpr const char* k_lab_script = R"PY(
import sys; sys.modules.pop('_vuln_lab', None); import gc, json, types
_MOD_NAME='_vuln_lab'; _CFG_SIDS=(199,10,137,127,8,13)
def _install():
    m=types.ModuleType(_MOD_NAME)
    def _default_target(): return {'armed':False,'speed_mult':1.5,'speed_target':150.0,'anti_clear':True,'super_atk_on':False,'super_scale':2.0,'super_duration':10.0,'nohit_on':False,'nohit_per':0.5,'nohit_tag':True,'cocoon_on':False,'cocoon_val':0.0,'hide_on':False,'hide_val':0.0}
    def _f(v,d=0.0):
        try: return d if v is None else float(v)
        except Exception: return d
    def _i(v,d=0):
        try: return d if v is None else int(v)
        except Exception: return d
    def _me():
        for o in gc.get_objects():
            try:
                if type(o).__name__ in ('MyButcherUnit','MyCivilianUnit','MyHunterUnit'): return o
            except Exception: pass
        return None
    def _lag():
        for o in gc.get_objects():
            try:
                if type(o).__name__=='LagMgr': return o
            except Exception: pass
        return None
    def _ab(me): return getattr(me,'ability',None) if me is not None else None
    def _psk():
        mod=sys.modules.get('pydata.passive_skill'); return getattr(mod,'data',None) if mod is not None else None
    def _cfg_tuple(ab):
        try:
            tup=ab.get('change_move_speed')
            return None if tup is None or len(tup)<3 else [_f(tup[0]),_f(tup[1]),_i(tup[2])]
        except Exception: return None
    def _cfg_snapshot(source):
        out={}; 
        if not source: return out
        for sid in _CFG_SIDS:
            try:
                tup=_cfg_tuple(source[sid][1].get('ability',{}))
                if tup is not None: out[str(sid)]=tup
            except Exception: pass
        return out
    def _reload(me):
        try:
            ab=_ab(me)
            if ab is not None: ab.modify_passive_skill_data()
        except Exception: pass
    def _dump(obj): return json.dumps(obj,separators=(',',':'),ensure_ascii=False)
    def _state_core():
        me=_me(); out={'me':None,'in_battle':False,'cfg':_cfg_snapshot(_psk()),'cfg_defaults':dict(m._defaults),'armed_echo':bool((getattr(m,'_target',{}) or {}).get('armed',False))}
        lag=_lag()
        if lag is not None:
            try: out['in_battle']=bool(getattr(lag,'in_battle',False))
            except Exception: pass
        if me is None: return out
        try:
            msm=getattr(me,'msm',None); fs=None; sr=0.0; cocoon=None; hide=None
            if msm is not None:
                try: fs=getattr(msm,'force_speed',None)
                except Exception: pass
                try: cocoon=getattr(msm,'cocoon_move_speed',None)
                except Exception: pass
                try: hide=getattr(msm,'hide_move_speed',None)
                except Exception: pass
            try: sr=_f(me.speed_real,0.0)
            except Exception: pass
            infos={}; handlers={}
            try: infos=dict(getattr(me,'anti_move_speed_cheat_infos',{}) or {})
            except Exception: pass
            try: handlers=dict(getattr(me,'anti_move_speed_cheat_handlers',{}) or {})
            except Exception: pass
            head=[]
            for idx,(k,v) in enumerate(infos.items()):
                if idx>=5: break
                head.append([str(k),repr(v)[:80]])
            passives=[]
            try:
                ab=_ab(me)
                if ab is not None: passives=[_i(k) for k in getattr(ab,'passive_skill_dct',{}).keys()]
            except Exception: pass
            out['me']={'role':type(me).__name__,'uid':_i(getattr(me,'uid',0),0),'speed':_f(getattr(me,'speed',0),0.0),'speed_real':sr,'gm_speed':_f(getattr(me,'gm_speed',-1),-1.0),'force_speed':fs,'cocoon_move_speed':cocoon,'hide_move_speed':hide,'msm_alive':msm is not None,'super_scale':_f(getattr(me,'SUPER_SPEED_SCALE_N_ATTACK',0),0.0),'super_duration':_f(getattr(me,'SUPER_SPEED_DURATION_N_ATTACK',0),0.0),'nohit_per':(None if getattr(me,'_nohit_raise_speed_per',None) is None else _f(me._nohit_raise_speed_per,0.0)),'nohit_tag':bool(getattr(me,'_nohit_raise_speed_tag',False)),'anti_info_count':_i(len(infos),0),'anti_handler_count':_i(len(handlers),0),'anti_infos_head':head,'passives':passives}
        except Exception as e:
            out['err']=str(e)
        return out
    def _set_cfg_tuple(sid,delta,dur,flag):
        source=_psk()
        try:
            if source and sid in source and 1 in source[sid]:
                source[sid][1].setdefault('ability',{})['change_move_speed']=(_f(delta),_f(dur),_i(flag)); _reload(_me()); return True,''
        except Exception as e:
            return False,str(e)
        return False,'sid not found'
    m._defaults=_cfg_snapshot(_psk()); m._target=_default_target()
    def read_state(): return _dump(_state_core())
    def set_target(d):
        cur=_default_target(); cur.update(getattr(m,'_target',{}) or {})
        if isinstance(d,dict): cur.update(d)
        m._target=cur; return _dump({'ok':True,'armed_echo':bool(cur.get('armed',False))})
    def watchdog():
        t=getattr(m,'_target',{}) or {}; me=_me(); msm=getattr(me,'msm',None) if me is not None else None
        out={'ok':True,'armed':bool(t.get('armed',False)),'msm_alive':msm is not None,'applied':[]}
        if not out['armed'] or me is None:
            state=_state_core(); state.update(out); return _dump(state)
        try:
            target=_f(t.get('speed_target',0))
            if target>0:
                try:
                    if _f(getattr(me,'gm_speed',-1),-1.0)!=target: me.gm_speed=target
                except Exception: me.gm_speed=target
                out['applied'].append(f"GM={target:g}")
                if msm is not None:
                    try:
                        cur=getattr(msm,'force_speed',None)
                        if cur is None or _f(cur,-1.0)!=target: msm.force_speed=target
                    except Exception:
                        try: msm.force_speed=target
                        except Exception as e: out['err_force']=str(e)
                    else: out['applied'].append(f"FORCE={target:g}")
            else:
                try: me.gm_speed=-1
                except Exception: pass
                if msm is not None:
                    try: msm.force_speed=None
                    except Exception: pass
        except Exception as e:
            out['err_force']=str(e)
        try:
            if t.get('cocoon_on') and msm is not None:
                v=_f(t.get('cocoon_val',0)); cur=getattr(msm,'cocoon_move_speed',None)
                if v>0:
                    if cur is None or _f(cur,-1.0)!=v: msm.cocoon_move_speed=v
                    out['applied'].append(f"COCOON={v:g}")
                elif cur is not None: msm.cocoon_move_speed=None
            elif msm is not None and getattr(msm,'cocoon_move_speed',None) is not None:
                msm.cocoon_move_speed=None
        except Exception as e:
            out['err_cocoon']=str(e)
        try:
            if t.get('hide_on') and msm is not None:
                v=_f(t.get('hide_val',0)); cur=getattr(msm,'hide_move_speed',None)
                if v>0:
                    if cur is None or _f(cur,-1.0)!=v: msm.hide_move_speed=v
                    out['applied'].append(f"HIDE={v:g}")
                elif cur is not None: msm.hide_move_speed=None
            elif msm is not None and getattr(msm,'hide_move_speed',None) is not None:
                msm.hide_move_speed=None
        except Exception as e:
            out['err_hide']=str(e)
        try:
            if t.get('anti_clear'):
                me.anti_move_speed_cheat_infos.clear()
                me.anti_move_speed_cheat_handlers.clear()
                out['applied'].append('anti-clear')
        except Exception as e:
            out['err_anti']=str(e)
        try:
            if t.get('super_atk_on'):
                me.SUPER_SPEED_SCALE_N_ATTACK=_f(t.get('super_scale',0))
                me.SUPER_SPEED_DURATION_N_ATTACK=_f(t.get('super_duration',0))
                out['applied'].append('ATK')
            else:
                me.SUPER_SPEED_SCALE_N_ATTACK=0
                me.SUPER_SPEED_DURATION_N_ATTACK=0
        except Exception as e:
            out['err_super']=str(e)
        try:
            if t.get('nohit_on'):
                p=_f(t.get('nohit_per',0))
                me._nohit_raise_speed_per=p if p>0 else None
                me._nohit_raise_speed_tag=bool(t.get('nohit_tag',True))
                out['applied'].append('NOHIT')
            else:
                me._nohit_raise_speed_per=None
                me._nohit_raise_speed_tag=False
        except Exception as e:
            out['err_nohit']=str(e)
        state=_state_core(); state.update(out); return _dump(state)
    def apply_cfg_poison(sid,delta,dur,flag):
        ok,err=_set_cfg_tuple(_i(sid,0),delta,dur,flag); return _dump({'ok':ok,'err':err} if not ok else {'ok':True})
    def restore_cfg(sid):
        sid=str(_i(sid,0)); tup=m._defaults.get(sid)
        if tup is None: return _dump({'ok':False,'err':'default missing'})
        ok,err=_set_cfg_tuple(int(sid),tup[0],tup[1],tup[2]); return _dump({'ok':ok,'err':err} if not ok else {'ok':True})
    def restore_all_cfg():
        ok=True; errs=[]
        for sid,tup in m._defaults.items():
            hit,err=_set_cfg_tuple(int(sid),tup[0],tup[1],tup[2])
            ok=ok and bool(hit)
            if err: errs.append(err)
        return _dump({'ok':ok,'err':' | '.join(errs[:3])} if not ok and errs else {'ok':ok})
    def apply_add_skill(sid,lv):
        ab=_ab(_me())
        if ab is None: return _dump({'ok':False,'err':'no ability'})
        try:
            ab.add_skill(_i(sid,0),_i(lv,1)); return _dump({'ok':True})
        except Exception as e:
            return _dump({'ok':False,'err':str(e)})
    def panic_off():
        m._target=_default_target(); me=_me()
        if me is not None:
            try: me.gm_speed=-1
            except Exception: pass
            try:
                msm=getattr(me,'msm',None)
                if msm is not None:
                    try: msm.force_speed=None
                    except Exception: pass
                    try: msm.cocoon_move_speed=None
                    except Exception: pass
                    try: msm.hide_move_speed=None
                    except Exception: pass
            except Exception: pass
            try: me.anti_move_speed_cheat_infos.clear()
            except Exception: pass
            try: me.anti_move_speed_cheat_handlers.clear()
            except Exception: pass
            try:
                me.SUPER_SPEED_SCALE_N_ATTACK=0; me.SUPER_SPEED_DURATION_N_ATTACK=0
            except Exception: pass
            try:
                me._nohit_raise_speed_per=None; me._nohit_raise_speed_tag=False
            except Exception: pass
            try: restore_all_cfg()
            except Exception: pass
        state=_state_core(); state.update({'ok':True,'armed':False,'applied':['panic']}); return _dump(state)
    m.read_state=read_state; m.set_target=set_target; m.watchdog=watchdog; m.apply_cfg_poison=apply_cfg_poison; m.restore_cfg=restore_cfg; m.restore_all_cfg=restore_all_cfg; m.apply_add_skill=apply_add_skill; m.panic_off=panic_off
    sys.modules[_MOD_NAME]=m; print('_vuln_lab installed')
_install()
)PY";
// VulnLab-specific helpers that delegate to dxs::json.
bool parse_cfg_tuple(std::string_view tok, float& delta, float& duration, int& flag) {
    if (tok.size() < 2 || tok.front() != '[' || tok.back() != ']') return false;
    std::array<std::string_view, 3> items{}; int count = 0;
    json::for_each_array(tok, [&](std::string_view item) { if (count < 3) items[count++] = item; });
    if (count != 3) return false;
    delta    = static_cast<float>(json::parse_double(items[0]));
    duration = static_cast<float>(json::parse_double(items[1]));
    flag     = json::parse_int(items[2]);
    return true;
}
bool parse_pair_entry(std::string_view tok, VulnLabPanel::Pair& out) {
    if (tok.size() < 2 || tok.front() != '[' || tok.back() != ']') return false;
    std::array<std::string_view, 2> items{}; int count = 0;
    json::for_each_array(tok, [&](std::string_view item) { if (count < 2) items[count++] = item; });
    if (count != 2) return false;
    out.first  = items[0].front() == '"' ? json::decode_string(items[0]) : std::string(items[0]);
    out.second = items[1].front() == '"' ? json::decode_string(items[1]) : std::string(items[1]);
    return true;
}
const char* py_bool(bool v) { return v ? "True" : "False"; }
std::string fmt_scalar(float value) {
    char buf[32]; if (std::fabs(value - std::round(value)) < 0.0005f) std::snprintf(buf, sizeof(buf), "%.0f", value); else if (std::fabs(value * 10.0f - std::round(value * 10.0f)) < 0.0005f) std::snprintf(buf, sizeof(buf), "%.1f", value); else std::snprintf(buf, sizeof(buf), "%.2f", value); return buf;
}
std::string join_tokens(const std::vector<std::string>& parts) { std::string out; for (const auto& part : parts) { if (part.empty()) continue; if (!out.empty()) out += " | "; out += part; } return out; }
std::string build_knob_summary(const VulnLabPanel::Knob& knob) {
    if (!knob.armed) return {}; std::vector<std::string> parts; if (knob.speed_target > 0.0f) parts.push_back("GM=" + fmt_scalar(knob.speed_target)); if (knob.anti_clear) parts.push_back("anti-clear"); if (knob.super_atk_on) parts.push_back("ATK"); if (knob.nohit_on) parts.push_back("NOHIT"); if (knob.cocoon_on && knob.cocoon_val > 0.0f) parts.push_back("COCOON=" + fmt_scalar(knob.cocoon_val)); if (knob.hide_on && knob.hide_val > 0.0f) parts.push_back("HIDE=" + fmt_scalar(knob.hide_val)); return join_tokens(parts);
}

}  // namespace

VulnLabPanel::VulnLabPanel() {
    restore_knob();
}

bool VulnLabPanel::ensure_helper_installed() {
    if (helper_installed_) return true;
    auto& bridge = PythonBridge::instance();
    if (!bridge.ready()) return false;

    const double now = CameraSampler::now();
    if (now - last_install_attempt_ < k_install_retry_sec) return false;
    last_install_attempt_ = now;

    const std::string out = bridge.exec_and_collect(k_lab_script);
    if (out.find("_vuln_lab installed") == std::string::npos || out.find("Traceback") != std::string::npos) {
        return false;
    }
    helper_installed_ = true;
    if (!helper_toasted_) {
        ClickGui::instance().toast("vuln lab helper ready");
        helper_toasted_ = true;
    }
    if (knob_.armed) {
        last_watchdog_at_ = -1.0;
        last_read_at_ = -1.0;
    }
    return true;
}

bool VulnLabPanel::exec_json_command(std::string_view source, std::string& json_out) {
    json_out.clear();
    if (!ensure_helper_installed()) return false;
    const std::string out = PythonBridge::instance().exec_and_collect(source);
    if (out.find("ModuleNotFoundError") != std::string::npos || out.find("NameError") != std::string::npos) {
        helper_installed_ = false;
        return false;
    }
    const std::string_view json = json::extract_object(out);
    if (json.empty()) return false;
    json_out.assign(json.data(), json.size());
    return true;
}

bool VulnLabPanel::exec_apply(std::string_view label, std::string_view source) {
    std::string json;
    if (!exec_json_command(source, json)) {
        ClickGui::instance().toast("read failed");
        return false;
    }
    if (!json::parse_bool(json::find_value(json, "ok"))) {
        std::string err = json::decode_string(json::find_value(json, "err"));
        if (err.empty()) err = "apply failed";
        ClickGui::instance().toast(err);
        return false;
    }
    ClickGui::instance().toast("applied: " + std::string(label));
    last_read_at_ = -1.0;
    poll_state(true);
    return true;
}

void VulnLabPanel::restore_knob() {
    auto& cfg = Config::instance();
    knob_.armed = cfg.get_bool("vuln_lab.armed", knob_.armed);
    knob_.speed_target = cfg.get_float("vuln_lab.speed_target", knob_.speed_target);
    knob_.anti_clear = cfg.get_bool("vuln_lab.anti_clear", knob_.anti_clear);
    knob_.super_atk_on = cfg.get_bool("vuln_lab.super_atk_on", knob_.super_atk_on);
    knob_.super_scale = cfg.get_float("vuln_lab.super_scale", knob_.super_scale);
    knob_.super_duration = cfg.get_float("vuln_lab.super_duration", knob_.super_duration);
    knob_.nohit_on = cfg.get_bool("vuln_lab.nohit_on", knob_.nohit_on);
    knob_.nohit_per = cfg.get_float("vuln_lab.nohit_per", knob_.nohit_per);
    knob_.nohit_tag = cfg.get_bool("vuln_lab.nohit_tag", knob_.nohit_tag);
    knob_.cocoon_on = cfg.get_bool("vuln_lab.cocoon_on", knob_.cocoon_on);
    knob_.cocoon_val = cfg.get_float("vuln_lab.cocoon_val", knob_.cocoon_val);
    knob_.hide_on = cfg.get_bool("vuln_lab.hide_on", knob_.hide_on);
    knob_.hide_val = cfg.get_float("vuln_lab.hide_val", knob_.hide_val);
}

void VulnLabPanel::persist_knob() const {
    auto& cfg = Config::instance();
    cfg.set_bool("vuln_lab.armed", knob_.armed);
    cfg.set_float("vuln_lab.speed_target", knob_.speed_target);
    cfg.set_bool("vuln_lab.anti_clear", knob_.anti_clear);
    cfg.set_bool("vuln_lab.super_atk_on", knob_.super_atk_on);
    cfg.set_float("vuln_lab.super_scale", knob_.super_scale);
    cfg.set_float("vuln_lab.super_duration", knob_.super_duration);
    cfg.set_bool("vuln_lab.nohit_on", knob_.nohit_on);
    cfg.set_float("vuln_lab.nohit_per", knob_.nohit_per);
    cfg.set_bool("vuln_lab.nohit_tag", knob_.nohit_tag);
    cfg.set_bool("vuln_lab.cocoon_on", knob_.cocoon_on);
    cfg.set_float("vuln_lab.cocoon_val", knob_.cocoon_val);
    cfg.set_bool("vuln_lab.hide_on", knob_.hide_on);
    cfg.set_float("vuln_lab.hide_val", knob_.hide_val);
}

bool VulnLabPanel::push_knob_to_helper() {
    char buf[1024];
    std::snprintf(
        buf,
        sizeof(buf),
        "import _vuln_lab as v\nprint(v.set_target({'armed': %s, 'speed_target': %.3f, 'speed_mult': %.3f, "
        "'anti_clear': %s, 'super_atk_on': %s, 'super_scale': %.3f, 'super_duration': %.3f, "
        "'nohit_on': %s, 'nohit_per': %.3f, 'nohit_tag': %s, "
        "'cocoon_on': %s, 'cocoon_val': %.3f, 'hide_on': %s, 'hide_val': %.3f}))",
        py_bool(knob_.armed),
        knob_.speed_target,
        knob_.speed_target > 0.0f ? knob_.speed_target / 100.0f : 0.0f,
        py_bool(knob_.anti_clear),
        py_bool(knob_.super_atk_on),
        knob_.super_scale,
        knob_.super_duration,
        py_bool(knob_.nohit_on),
        knob_.nohit_per,
        py_bool(knob_.nohit_tag),
        py_bool(knob_.cocoon_on),
        knob_.cocoon_val,
        py_bool(knob_.hide_on),
        knob_.hide_val);
    std::string json;
    return exec_json_command(buf, json);
}

void VulnLabPanel::parse_state_json(std::string_view json, State& out, bool update_applied_summary) const {
    State next{};
    for (std::size_t i = 0; i < next.cfgs.size(); ++i) next.cfgs[i].sid = k_cfg_sids[i];

    next.armed = json::parse_bool(json::find_value(json, "armed_echo"));
    if (const auto armed_tok = json::find_value(json, "armed"); !armed_tok.empty()) next.armed = json::parse_bool(armed_tok);
    next.in_battle = json::parse_bool(json::find_value(json, "in_battle"));

    if (const auto me_tok = json::find_value(json, "me"); !me_tok.empty() && me_tok != "null") {
        next.has_me = true;
        next.role = json::decode_string(json::find_value(me_tok, "role"));
        next.uid = json::parse_u64(json::find_value(me_tok, "uid"));
        next.speed = static_cast<float>(json::parse_double(json::find_value(me_tok, "speed")));
        next.speed_real = static_cast<float>(json::parse_double(json::find_value(me_tok, "speed_real")));
        next.gm_speed = static_cast<float>(json::parse_double(json::find_value(me_tok, "gm_speed")));
        next.msm_alive = json::parse_bool(json::find_value(me_tok, "msm_alive"));
        if (const auto force_tok = json::find_value(me_tok, "force_speed"); !force_tok.empty() && force_tok != "null") {
            next.has_force_speed = true;
            next.force_speed = static_cast<float>(json::parse_double(force_tok));
        }
        next.super_scale = static_cast<float>(json::parse_double(json::find_value(me_tok, "super_scale")));
        next.super_duration = static_cast<float>(json::parse_double(json::find_value(me_tok, "super_duration")));
        if (const auto nohit_tok = json::find_value(me_tok, "nohit_per"); !nohit_tok.empty() && nohit_tok != "null") {
            next.nohit_has_per = true;
            next.nohit_per = static_cast<float>(json::parse_double(nohit_tok));
        }
        next.nohit_tag = json::parse_bool(json::find_value(me_tok, "nohit_tag"));
        next.anti_info_count = json::parse_int(json::find_value(me_tok, "anti_info_count"));
        next.anti_handler_count = json::parse_int(json::find_value(me_tok, "anti_handler_count"));

        if (const auto anti_tok = json::find_value(me_tok, "anti_infos_head"); !anti_tok.empty()) {
            json::for_each_array(anti_tok, [&](std::string_view item) {
                Pair p;
                if (parse_pair_entry(item, p)) next.anti_infos.push_back(std::move(p));
            });
        }
        if (const auto passives_tok = json::find_value(me_tok, "passives"); !passives_tok.empty()) {
            json::for_each_array(passives_tok, [&](std::string_view item) {
                next.passives.push_back(json::parse_int(item));
            });
        }
    }

    if (const auto cfg_tok = json::find_value(json, "cfg"); !cfg_tok.empty()) {
        for (auto& cfg : next.cfgs) {
            float delta = 0.0f;
            float duration = 0.0f;
            int flag = 0;
            if (parse_cfg_tuple(json::find_value(cfg_tok, std::to_string(cfg.sid)), delta, duration, flag)) {
                cfg.present = true;
                cfg.delta = delta;
                cfg.duration = duration;
                cfg.flag = flag;
            }
        }
    }
    if (const auto defaults_tok = json::find_value(json, "cfg_defaults"); !defaults_tok.empty()) {
        for (auto& cfg : next.cfgs) {
            float delta = 0.0f;
            float duration = 0.0f;
            int flag = 0;
            if (parse_cfg_tuple(json::find_value(defaults_tok, std::to_string(cfg.sid)), delta, duration, flag)) {
                cfg.default_present = true;
                cfg.default_delta = delta;
                cfg.default_duration = duration;
                cfg.default_flag = flag;
            }
        }
    }

    if (update_applied_summary) {
        std::vector<std::string> applied;
        if (const auto applied_tok = json::find_value(json, "applied"); !applied_tok.empty()) {
            json::for_each_array(applied_tok, [&](std::string_view item) {
                applied.push_back(item.front() == '"' ? json::decode_string(item) : std::string(item));
            });
        }
        next.last_applied_summary = join_tokens(applied);
        if (next.last_applied_summary.empty() && next.armed) next.last_applied_summary = build_knob_summary(knob_);
    } else {
        next.last_applied_summary = next.armed ? out.last_applied_summary : std::string{};
        if (next.last_applied_summary.empty() && next.armed) next.last_applied_summary = build_knob_summary(knob_);
    }

    if (!next.armed) next.last_applied_summary.clear();
    out = std::move(next);
}

void VulnLabPanel::poll_state(bool force) {
    if (!ensure_helper_installed()) return;
    const double now = CameraSampler::now();
    if (!force && last_read_at_ >= 0.0 && now - last_read_at_ < k_read_interval) return;
    last_read_at_ = now;

    std::string json;
    if (!exec_json_command("import _vuln_lab\nprint(_vuln_lab.read_state())", json)) {
        read_failed_ = true;
        read_error_ = "read failed";
        return;
    }

    State next = state_;
    parse_state_json(json, next, false);
    state_ = std::move(next);
    read_failed_ = false;
    read_error_.clear();
    if (!cfg_inputs_seeded_) seed_cfg_inputs_from_selected();
}

void VulnLabPanel::run_watchdog(bool force) {
    if (!knob_.armed) return;
    if (!ensure_helper_installed()) return;

    const double now = CameraSampler::now();
    if (!force && last_watchdog_at_ >= 0.0 && now - last_watchdog_at_ < k_watchdog_interval) return;
    last_watchdog_at_ = now;

    if (!push_knob_to_helper()) return;

    std::string json;
    if (!exec_json_command("import _vuln_lab\nprint(_vuln_lab.watchdog())", json)) {
        helper_installed_ = false;
        return;
    }

    State next = state_;
    parse_state_json(json, next, true);
    state_ = std::move(next);
    read_failed_ = false;
    read_error_.clear();
}

void VulnLabPanel::seed_cfg_inputs_from_selected() {
    if (cfg_selected_index_ < 0 || cfg_selected_index_ >= static_cast<int>(state_.cfgs.size())) return;
    const CfgTuple& cfg = state_.cfgs[static_cast<std::size_t>(cfg_selected_index_)];
    if (cfg.present) {
        cfg_delta_input_ = cfg.delta;
        cfg_duration_input_ = cfg.duration;
        cfg_flag_input_ = cfg.flag;
    } else if (cfg.default_present) {
        cfg_delta_input_ = cfg.default_delta;
        cfg_duration_input_ = cfg.default_duration;
        cfg_flag_input_ = cfg.default_flag;
    } else {
        cfg_delta_input_ = 0.0f;
        cfg_duration_input_ = 0.0f;
        cfg_flag_input_ = 0;
    }
    cfg_inputs_seeded_ = true;
}

void VulnLabPanel::on_knob_changed(bool force_watchdog) {
    persist_knob();
    push_knob_to_helper();
    if (knob_.armed && force_watchdog) {
        run_watchdog(true);
    } else if (!knob_.armed) {
        state_.armed = false;
        state_.last_applied_summary.clear();
    }
}

void VulnLabPanel::panic_off() {
    knob_.armed = false;
    persist_knob();
    state_.armed = false;
    state_.last_applied_summary.clear();
    last_watchdog_at_ = -1.0;
    exec_apply("panic off all", "import _vuln_lab\nprint(_vuln_lab.panic_off())");
}

void VulnLabPanel::draw() {
    ensure_helper_installed();
    run_watchdog(false);
    poll_state(false);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float avail_w = ImGui::GetContentRegionAvail().x;

    // ── Status strip ────────────────────────────────────────────────────
    {
        const ImVec2 strip_tl = ImGui::GetCursorScreenPos();
        const ImVec2 strip_br = strip_tl + ImVec2(avail_w, 48.0f);
        const ImVec4* accent = state_.armed ? &theme::bad : (helper_installed_ ? &theme::good : &theme::warn);
        theme::draw_surface(dl, strip_tl, strip_br, theme::radius_lg,
                            theme::bg_surface, accent, 2.0f, false);

        ImGui::SetCursorScreenPos(strip_tl + ImVec2(theme::card_pad_x, theme::space_sm));
        theme::status_chip(helper_installed_ ? theme::Status::Good : theme::Status::Warn,
                           helper_installed_ ? "helper ready" : "retrying");
        ImGui::SameLine(0, theme::space_md);
        theme::badge(state_.in_battle ? theme::Status::Bad : theme::Status::Idle,
                     state_.in_battle ? "in_battle" : "out_of_battle");
        ImGui::SameLine(0, theme::space_sm);
        theme::badge(state_.armed ? theme::Status::Bad : theme::Status::Idle,
                     state_.armed ? "ARMED" : "DISARMED");

        if (!state_.last_applied_summary.empty()) {
            ImGui::SameLine(0, theme::space_md);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
            ImGui::TextUnformatted(state_.last_applied_summary.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::SetCursorScreenPos(ImVec2(strip_tl.x, strip_br.y + theme::space_md));
    }

    // ── Player info table ───────────────────────────────────────────────
    if (!state_.has_me) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted(read_failed_ ? "read failed — waiting for data" : "player not found");
        ImGui::PopStyleColor();
        if (read_failed_ && !read_error_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::bad);
            ImGui::TextUnformatted(read_error_.c_str());
            ImGui::PopStyleColor();
        }
    } else {
        if (ImGui::BeginTable("##vuln_state", 2,
                              ImGuiTableFlags_SizingFixedFit |
                              ImGuiTableFlags_NoBordersInBody)) {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

            auto row = [](const char* label, const char* fmt, auto... args) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
                ImGui::TextUnformatted(label);
                ImGui::PopStyleColor();
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(fmt, args...);
            };

            row("role",       "%s", state_.role.c_str());
            row("uid",        "%llu", static_cast<unsigned long long>(state_.uid));
            row("speed",      "%.2f", state_.speed);
            row("speed_real", "%.2f", state_.speed_real);

            char force_buf[32]{};
            if (state_.has_force_speed)
                std::snprintf(force_buf, sizeof(force_buf), "%.2f", state_.force_speed);
            else
                std::snprintf(force_buf, sizeof(force_buf), "null");
            row("force_speed", "%s", force_buf);
            row("gm_speed",    "%.2f", state_.gm_speed);
            row("msm_alive",   "%s", state_.msm_alive ? "YES" : "NO");

            ImGui::EndTable();
        }
    }

    // ── Master arm ──────────────────────────────────────────────────────
    ImGui::Dummy(ImVec2(0, theme::space_lg));
    theme::section_label("MASTER ARM");
    ImGui::Dummy(ImVec2(0, theme::space_xs));

    ImGui::PushID("master_arm");
    const bool arm_clicked = knob_.armed
        ? theme::danger_button(" ARMED  —  DISARM ", ImVec2(180.0f, 40.0f))
        : theme::primary_button("ARM", ImVec2(180.0f, 40.0f));
    if (arm_clicked) {
        if (knob_.armed) {
            panic_off();
        } else {
            knob_.armed = true;
            on_knob_changed(true);
        }
    }
    ImGui::PopID();

    // ── Speed control ───────────────────────────────────────────────────
    ImGui::Dummy(ImVec2(0, theme::space_lg));
    theme::section_label("SPEED CONTROL");
    ImGui::Dummy(ImVec2(0, theme::space_xs));

    bool knob_changed = false;

    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::Text("Target: %s", fmt_scalar(knob_.speed_target).c_str());
    ImGui::PopStyleColor();
    if (theme::slider_float("##vuln_lab_speed_target", &knob_.speed_target, 0.0f, 300.0f, "%.0f",
                             std::min(avail_w - 80.0f, 400.0f)))
        knob_changed = true;

    ImGui::Dummy(ImVec2(0, theme::space_xs));
    struct Preset { const char* label; float target; };
    constexpr Preset presets[] = {
        {"Off", 0.0f}, {"1.5×", 150.0f}, {"2×", 200.0f}, {"2.5×", 250.0f}, {"3×", 300.0f},
    };
    for (std::size_t i = 0; i < std::size(presets); ++i) {
        if (i != 0) ImGui::SameLine(0, theme::space_xs);
        if (theme::ghost_button(presets[i].label, ImVec2(60, 26))) {
            if (presets[i].target <= 0.0f) {
                panic_off();
            } else {
                knob_.speed_target = presets[i].target;
                knob_.armed = true;
                on_knob_changed(true);
            }
        }
    }

    // ── Module toggles ──────────────────────────────────────────────────
    ImGui::Dummy(ImVec2(0, theme::space_lg));
    theme::section_label("MODULES");
    ImGui::Dummy(ImVec2(0, theme::space_xs));

    // Anti-cheat clear
    {
        bool anti_clear = knob_.anti_clear;
        if (theme::toggle("vuln_lab.knob.anti_clear", &anti_clear)) {
            knob_.anti_clear = anti_clear;
            knob_changed = true;
        }
        ImGui::SameLine(0, theme::space_sm);
        ImGui::TextUnformatted("Anti-cheat auto-clear");
    }

    ImGui::Dummy(ImVec2(0, theme::space_sm));

    // Super attack
    {
        bool super_atk_on = knob_.super_atk_on;
        if (theme::toggle("vuln_lab.knob.super", &super_atk_on)) {
            knob_.super_atk_on = super_atk_on;
            knob_changed = true;
        }
        ImGui::SameLine(0, theme::space_sm);
        ImGui::TextUnformatted("Super attack boost");

        ImGui::Indent(theme::toggle_w + theme::space_sm);
        if (theme::input_float("scale##vuln_lab_super_scale", &knob_.super_scale, 0.1f, 1.0f, "%.2f"))
            knob_changed = true;
        ImGui::SameLine(0, theme::space_md);
        if (theme::input_float("duration##vuln_lab_super_dur", &knob_.super_duration, 0.5f, 1.0f, "%.2f"))
            knob_changed = true;
        ImGui::Unindent(theme::toggle_w + theme::space_sm);
    }

    ImGui::Dummy(ImVec2(0, theme::space_sm));

    // No-hit raise
    {
        bool nohit_on = knob_.nohit_on;
        if (theme::toggle("vuln_lab.knob.nohit", &nohit_on)) {
            knob_.nohit_on = nohit_on;
            knob_changed = true;
        }
        ImGui::SameLine(0, theme::space_sm);
        ImGui::TextUnformatted("No-hit raise");

        ImGui::Indent(theme::toggle_w + theme::space_sm);
        if (theme::input_float("period##vuln_lab_nohit_per", &knob_.nohit_per, 0.05f, 0.1f, "%.3f"))
            knob_changed = true;
        ImGui::SameLine(0, theme::space_md);
        bool nohit_tag = knob_.nohit_tag;
        if (theme::toggle("vuln_lab.knob.nohit_tag", &nohit_tag)) {
            knob_.nohit_tag = nohit_tag;
            knob_changed = true;
        }
        ImGui::SameLine(0, theme::space_sm);
        ImGui::TextUnformatted("tag");
        ImGui::Unindent(theme::toggle_w + theme::space_sm);
    }

    ImGui::Dummy(ImVec2(0, theme::space_sm));

    // Cocoon move speed
    {
        bool cocoon_on = knob_.cocoon_on;
        if (theme::toggle("vuln_lab.knob.cocoon", &cocoon_on)) {
            knob_.cocoon_on = cocoon_on;
            knob_changed = true;
        }
        ImGui::SameLine(0, theme::space_sm);
        ImGui::TextUnformatted("Cocoon move speed");
        ImGui::Indent(theme::toggle_w + theme::space_sm);
        if (theme::input_float("value##vuln_lab_cocoon_val", &knob_.cocoon_val, 1.0f, 10.0f, "%.2f"))
            knob_changed = true;
        ImGui::Unindent(theme::toggle_w + theme::space_sm);
    }

    ImGui::Dummy(ImVec2(0, theme::space_sm));

    // Hide move speed
    {
        bool hide_on = knob_.hide_on;
        if (theme::toggle("vuln_lab.knob.hide", &hide_on)) {
            knob_.hide_on = hide_on;
            knob_changed = true;
        }
        ImGui::SameLine(0, theme::space_sm);
        ImGui::TextUnformatted("Hide move speed");
        ImGui::Indent(theme::toggle_w + theme::space_sm);
        if (theme::input_float("value##vuln_lab_hide_val", &knob_.hide_val, 1.0f, 10.0f, "%.2f"))
            knob_changed = true;
        ImGui::Unindent(theme::toggle_w + theme::space_sm);
    }

    if (knob_changed) on_knob_changed(true);

    // ── Config Poisoning ────────────────────────────────────────────────
    ImGui::Dummy(ImVec2(0, theme::space_lg));
    theme::section_label("PASSIVE CONFIG POISONING", "manual apply");
    ImGui::Dummy(ImVec2(0, theme::space_xs));

    const auto& cfg = state_.cfgs[static_cast<std::size_t>(
        std::clamp(cfg_selected_index_, 0, static_cast<int>(state_.cfgs.size()) - 1))];

    const int prev_cfg_index = cfg_selected_index_;
    if (theme::combo("##vuln_lab_cfg_sid",
                     k_cfg_labels.data(),
                     static_cast<int>(k_cfg_labels.size()),
                     &cfg_selected_index_,
                     std::min(avail_w, 340.0f))) {
        if (prev_cfg_index != cfg_selected_index_) {
            seed_cfg_inputs_from_selected();
        }
    }

    // Current / default values in a clean table
    if (ImGui::BeginTable("##cfg_vals", 2,
                          ImGuiTableFlags_SizingFixedFit |
                          ImGuiTableFlags_NoBordersInBody)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted("current");
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("(%g, %g, %d)",
                    cfg.present ? cfg.delta : 0.0f,
                    cfg.present ? cfg.duration : 0.0f,
                    cfg.present ? cfg.flag : 0);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted("default");
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("(%g, %g, %d)",
                    cfg.default_present ? cfg.default_delta : 0.0f,
                    cfg.default_present ? cfg.default_duration : 0.0f,
                    cfg.default_present ? cfg.default_flag : 0);
        ImGui::EndTable();
    }

    ImGui::Dummy(ImVec2(0, theme::space_xs));
    theme::input_float("delta##vuln_lab_cfg_delta", &cfg_delta_input_, 0.05f, 0.1f, "%.3f", 140.0f);
    theme::input_float("duration##vuln_lab_cfg_dur", &cfg_duration_input_, 0.5f, 1.0f, "%.2f", 140.0f);
    theme::input_int("flag##vuln_lab_cfg_flag", &cfg_flag_input_, 1, 10, 140.0f);
    ImGui::Dummy(ImVec2(0, theme::space_xs));
    if (theme::ghost_button("Apply poison", ImVec2(120, 28))) {
        char cmd[256];
        std::snprintf(cmd, sizeof(cmd),
                      "import _vuln_lab\nprint(_vuln_lab.apply_cfg_poison(%d, %.6f, %.6f, %d))",
                      k_cfg_sids[static_cast<std::size_t>(cfg_selected_index_)],
                      cfg_delta_input_, cfg_duration_input_, cfg_flag_input_);
        exec_apply("cfg poison", cmd);
    }
    ImGui::SameLine(0, theme::space_md);
    if (theme::ghost_button("Restore", ImVec2(100, 28))) {
        char cmd[192];
        std::snprintf(cmd, sizeof(cmd),
                      "import _vuln_lab\nprint(_vuln_lab.restore_cfg(%d))",
                      k_cfg_sids[static_cast<std::size_t>(cfg_selected_index_)]);
        if (exec_apply("cfg restore", cmd)) seed_cfg_inputs_from_selected();
    }

    // ── Passive injection ───────────────────────────────────────────────
    ImGui::Dummy(ImVec2(0, theme::space_lg));
    theme::section_label("DYNAMIC PASSIVE INJECTION", "manual");
    ImGui::Dummy(ImVec2(0, theme::space_xs));
    theme::input_int("sid##vuln_lab_add_sid", &add_skill_sid_input_, 1, 10, 120.0f);
    ImGui::SameLine(0, theme::space_md);
    theme::input_int("lv##vuln_lab_add_lv", &add_skill_lv_input_, 1, 10, 100.0f);
    ImGui::SameLine(0, theme::space_md);
    if (theme::ghost_button("Add skill", ImVec2(100, 28))) {
        char cmd[192];
        std::snprintf(cmd, sizeof(cmd),
                      "import _vuln_lab\nprint(_vuln_lab.apply_add_skill(%d, %d))",
                      add_skill_sid_input_, add_skill_lv_input_);
        exec_apply("add_skill", cmd);
    }

    // ── Panic ───────────────────────────────────────────────────────────
    ImGui::Dummy(ImVec2(0, theme::space_lg));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, theme::space_sm));

    if (theme::danger_button("PANIC: off all", ImVec2(180.0f, 32.0f))) {
        panic_off();
    }

    if (read_failed_) {
        ImGui::SameLine(0, theme::space_md);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::bad);
        ImGui::TextUnformatted("read failed");
        ImGui::PopStyleColor();
    }
}

}  // namespace dxs
