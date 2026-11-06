/*
 * WebStyles.h - Shared CSS for SkyView web interface
 */

#ifndef WEBSTYLES_H
#define WEBSTYLES_H

static const char WEB_CSS[] PROGMEM = R"css(
<style>
:root{
  --bg:#f0f2f5;--bg-card:#fff;--bg-header:#1a1a2e;--bg-tab:#fff;
  --bg-input:#f5f6fa;
  --text:#1a1a2e;--text-sec:#6b7280;--text-hdr:#fff;
  --text-tab:#6b7280;--text-tab-a:#4361ee;
  --border:#e5e7eb;--border-f:#4361ee;
  --primary:#4361ee;--primary-h:#3651d4;--primary-l:#eef0ff;
  --success:#10b981;--danger:#ef4444;
  --shadow:0 1px 3px rgba(0,0,0,.1);--shadow-lg:0 4px 12px rgba(0,0,0,.12);
  --r:12px;--r-sm:8px;--tr:.2s ease;
}
@media(prefers-color-scheme:dark){
  :root{
    --bg:#0f0f1a;--bg-card:#1a1a2e;--bg-header:#16162b;--bg-tab:#1a1a2e;
    --bg-input:#252540;
    --text:#e5e7eb;--text-sec:#9ca3af;--text-hdr:#fff;
    --text-tab:#9ca3af;--text-tab-a:#818cf8;
    --border:#2d2d4a;--border-f:#818cf8;
    --primary:#818cf8;--primary-h:#6d78e6;--primary-l:#1e1e3a;
    --success:#34d399;--danger:#f87171;
    --shadow:0 1px 3px rgba(0,0,0,.4);--shadow-lg:0 4px 12px rgba(0,0,0,.4);
  }
}
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
html{-webkit-text-size-adjust:100%;-webkit-tap-highlight-color:transparent}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Arial,sans-serif;
  font-size:15px;line-height:1.5;color:var(--text);background:var(--bg);
  min-height:100vh;overflow-x:hidden;padding-bottom:40px}

/* Header */
.hdr{background:var(--bg-header);color:var(--text-hdr);
  padding:10px 16px;display:flex;align-items:center;
  box-shadow:var(--shadow-lg);position:sticky;top:0;z-index:100}
.hdr-logo{width:32px;height:32px;border-radius:8px;flex-shrink:0;
  background:linear-gradient(135deg,#4361ee,#7c3aed);
  display:flex;align-items:center;justify-content:center;
  font-weight:700;font-size:14px;color:#fff;margin-right:10px}
.hdr-title{font-size:16px;font-weight:700}

/* Navigation tab bar */
.tab-bar{background:var(--bg-tab);display:flex;
  border-bottom:1px solid var(--border);box-shadow:var(--shadow);
  overflow-x:auto;scrollbar-width:none;
  position:sticky;top:52px;z-index:90}
.tab-bar::-webkit-scrollbar{display:none}
.tab-btn{flex:1;min-width:60px;padding:11px 6px;
  border:none;background:none;
  font-size:12px;font-weight:600;color:var(--text-tab);
  cursor:pointer;position:relative;
  transition:color var(--tr);white-space:nowrap;text-align:center;
  text-decoration:none;display:inline-block}
.tab-btn:hover{color:var(--text-tab-a);text-decoration:none}
.tab-btn.active{color:var(--text-tab-a)}
.tab-btn::after{content:'';position:absolute;bottom:0;left:50%;
  width:0;height:3px;background:var(--primary);
  border-radius:3px 3px 0 0;
  transition:all .3s cubic-bezier(.4,0,.2,1);transform:translateX(-50%)}
.tab-btn.active::after{width:60%}

/* Page content wrapper */
.page-content{padding:16px;max-width:640px;margin:0 auto;
  animation:fadeIn .2s ease}
@keyframes fadeIn{from{opacity:0;transform:translateY(5px)}to{opacity:1;transform:none}}

/* Cards */
.card{background:var(--bg-card);border-radius:var(--r);padding:16px;
  box-shadow:var(--shadow);border:1px solid var(--border);margin-bottom:12px}
.card-title{font-size:11px;font-weight:700;text-transform:uppercase;
  letter-spacing:.05em;color:var(--text-sec);margin-bottom:12px}

/* Rows (status display) */
.row{display:flex;justify-content:space-between;align-items:center;
  padding:8px 0;border-bottom:1px solid var(--border)}
.row:last-child{border-bottom:none;padding-bottom:0}
.row label{font-size:14px;color:var(--text-sec);flex-shrink:0;margin-right:8px}
.val{font-size:14px;font-weight:500;text-align:right}
.val.ok{color:var(--success)}
.val.warn{color:var(--danger)}

/* Form rows (settings) */
.form-row{padding:10px 0;border-bottom:1px solid var(--border)}
.form-row:last-of-type{border-bottom:none}
.form-row label{display:block;font-size:13px;font-weight:600;
  color:var(--text-sec);margin-bottom:5px}
select,input[type=text]{width:100%;padding:8px 10px;
  border:1px solid var(--border);border-radius:var(--r-sm);
  background:var(--bg-input);color:var(--text);font-size:14px;
  transition:border-color var(--tr),box-shadow var(--tr)}
select:focus,input[type=text]:focus{outline:none;
  border-color:var(--border-f);box-shadow:0 0 0 3px var(--primary-l)}

/* Buttons */
.btn{display:inline-flex;align-items:center;justify-content:center;
  padding:9px 18px;border:none;border-radius:20px;
  font-size:13px;font-weight:600;cursor:pointer;
  transition:all var(--tr);gap:6px;text-decoration:none}
.btn:hover{opacity:.85;text-decoration:none}
.btn-primary{background:var(--primary);color:#fff}
.btn-success{background:var(--success);color:#fff}
.btn-danger{background:var(--danger);color:#fff}
.btn-outline{background:none;border:1px solid var(--border);color:var(--text)}
.btn-sm{padding:6px 12px;font-size:12px}
.btn-full{width:100%;border-radius:var(--r-sm);margin-top:4px}
.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:10px}

/* Buddy / BLE list rows */
.buddy-row{display:flex;gap:8px;margin:6px 0;align-items:center}
.buddy-row input[name=hex]{width:90px;font-family:monospace;text-transform:uppercase}
.buddy-row input[name=nm]{flex:1}

/* Info text */
.info-text{font-size:13px;color:var(--text-sec);margin-bottom:10px}
code{font-family:monospace;background:var(--bg-input);
  padding:1px 5px;border-radius:4px;font-size:13px}

/* Status messages */
#status-msg{font-size:13px;font-weight:600;margin-top:10px;min-height:18px}
.status-ok{color:var(--success)}
.status-err{color:var(--danger)}

a{color:var(--primary);text-decoration:none}
a:hover{text-decoration:underline}
</style>
)css";

#endif /* WEBSTYLES_H */
