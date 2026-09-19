"""Draws the eight dash pages as HTML and renders them to the PNGs beside this
file.

    python3 docs/mockup/render.py

It needs Google Chrome (for the headless screenshot) and a network connection
(for Montserrat, the face LVGL draws with). Output goes to docs/mockup/*.png,
and the artboard sources for the design canvas to docs/mockup/build/*.dc.html.

This is a drawing of the firmware, not a screenshot of it, so it is only ever
as true as the numbers below. Every one of them is the number in the source:
the palette from dash_theme.hpp, the cell rectangles from each screen's own
create(), the status bar slots from dash_ui.cpp. A child's offsets are
measured from its parent's CONTENT box, which is what lv_obj_align and
lv_obj_set_pos measure from - the padding is already in them.

Change a layout in the firmware and this file has to be changed with it. It is
here rather than thrown away so that is a small job.
"""

import os
import subprocess
import sys

BG="#000000"; STATUS="#0A0A0A"; LINE="#343434"; TRACK="#161616"
TEXT="#F0F0F0"; DIM="#828282"; DOTOFF="#383838"; CAPTION="#E8C547"
GOOD="#57C08A"; WARN="#E8A33D"; CRIT="#E2504A"; CYAN="#58C7D6"
WARNBG="#271E0D"; WARNEDGE="#4A3A17"; WARNLABEL="#C0913F"
CRITBG="#3E100D"; CRITEDGE="#7A2320"; CRITLABEL="#E9A9A5"; CRITVALUE="#FFECEA"
CHROME=34   # 6 px shift strip + 28 px status bar
LH={14:16,20:24,28:32,36:40,48:52}

# LVGL draws LV_SYMBOL_WIFI from the glyph built into its Montserrat. Here it
# is the same arcs as inline SVG, at about the width that glyph occupies.
WIFI=('<svg viewBox="0 0 20 16" width="16" height="13" fill="none" stroke="#58C7D6"'
      ' stroke-width="1.6" stroke-linecap="round" style="vertical-align: -1px;">'
      '<path d="M2.2 5.4a12 12 0 0 1 15.6 0"/>'
      '<path d="M5.2 8.7a7.6 7.6 0 0 1 9.6 0"/>'
      '<path d="M8.2 12a3.2 3.2 0 0 1 3.6 0"/></svg>')
ARROW_UP='<span style="font-size: 11px;">&#9650;</span>'
ARROW_DOWN='<span style="font-size: 11px;">&#9660;</span>'

def esc(s): return s.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;")

def abspos(x,y,w=None,h=None,extra=""):
    s=f"position: absolute; left: {x}px; top: {y}px;"
    if w is not None: s+=f" width: {w}px;"
    if h is not None: s+=f" height: {h}px;"
    return s+" box-sizing: border-box; "+extra

def label(x,y,text,size,color,weight=500,w=None,align="left"):
    st=abspos(x,y,w)
    st+=(f"font-size: {size}px; line-height: {LH[size]}px; color: {color}; font-weight: {weight};"
         f" text-align: {align}; white-space: nowrap; overflow: hidden;")
    return f'<div style="{st}">{text}</div>'

# A label vertically centred in a row, the way LV_ALIGN_LEFT_MID centres one.
def midlabel(x,row_y,row_h,text,size,color,w=None,align="left"):
    return label(x,row_y+(row_h-LH[size])//2,text,size,color,w=w,align=align)

def block(x,y,w,h,color,extra=""):
    return f'<div style="{abspos(x,y,w,h, f"background: {color}; "+extra)}"></div>'

def frame(x,y,w,h,extra=""):
    return f'<div style="{abspos(x,y,w,h,extra)}">'

def chrome(page, link=("ONLINE",GOOD), alarm="", alarm_color=DIM,
           latch="", wifi="", clock="14:32", lit=0):
    out=[]
    for i in range(16):
        x0=(i*480)//16; x1=((i+1)*480)//16
        col=TRACK
        if i<lit: col = CRIT if i>=12 else (WARN if i>=8 else GOOD)
        out.append(block(x0,0,x1-x0-2,6,col))
    out.append(f'<div style="{abspos(0,6,480,28, f"background: {STATUS}; border-bottom: 1px solid {LINE};")}"></div>')
    for i in range(8):
        out.append(block(10+i*8,6+11,5,5, CYAN if i==page[1] else DOTOFF,"border-radius: 3px;"))
    out.append(label(78,12,page[0],14,TEXT))
    out.append(label(146,12,esc(alarm),14,alarm_color,w=114))
    out.append(label(268,12,latch,14,CRIT,w=36,align="right"))
    out.append(label(312,12,wifi,14,CYAN,w=34,align="right"))
    out.append(label(354,12,link[0],14,link[1],w=66,align="right"))
    out.append(label(428,12,clock,14,DIM,w=42,align="right"))
    return out

def tile(x,y,w,h,caption,unit,value,size,pad=8,sev=None,children=""):
    """One measurement cell, as ui_tile.cpp lays it out: caption top left, unit
       top right, value lifted four pixels off the bottom."""
    bg,edge,cap,val,un = BG,LINE,CAPTION,TEXT,DIM
    if sev=="warn": bg,edge,cap,val,un = WARNBG,WARNEDGE,WARNLABEL,WARN,WARNLABEL
    if sev=="crit": bg,edge,cap,val,un = CRITBG,CRITEDGE,CRITLABEL,CRITVALUE,CRITLABEL
    kids=[label(pad,pad,caption,14,cap)]
    if unit: kids.append(label(pad,pad,unit,14,un,w=w-2*pad,align="right"))
    if value: kids.append(label(pad,h-pad-4-LH[size],value,size,val))
    return (frame(x,y,w,h,f"background: {bg}; border-top: 1px solid {edge}; border-left: 1px solid {edge};")
            + "".join(kids) + children + "</div>")

def bar(x,y,w,h,fill_x,fill_w,fill_color,marks=()):
    kids=[block(0,0,w,h,TRACK), block(fill_x,0,fill_w,h,fill_color)]
    for mx,mw,mc in marks: kids.append(block(mx,0,mw,h,mc))
    return frame(x,y,w,h)+"".join(kids)+"</div>"

def page_wrap(parts):
    return frame(0,CHROME,480,286)+"".join(parts)+"</div>"

def doc(title, body):
    return f'''<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>{title}</title>
  <script src="./support.js"></script>
</head>
<body>
<x-dc>
<helmet>
  <link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Montserrat:wght@500;600&amp;display=swap">
  <style>
    body {{ margin: 0; background: {BG}; }}
  </style>
</helmet>
<div style="width: 480px; height: 320px; box-sizing: border-box; background: {BG}; position: relative; overflow: hidden; font-family: Montserrat, system-ui, sans-serif; -webkit-font-smoothing: antialiased;">
{body}
</div>
</x-dc>
<script type="text/x-dc" data-dc-script data-props='{{"$preview":{{"width":480,"height":320}}}}'>
class Component extends DCLogic {{
  renderVals() {{
    return {{}};
  }}
}}
</script>
</body>
</html>
'''

COL=160; ROW=95; PAD=8
def hero(x,y,caption,unit,value,tgt,fill,marks,sev=None):
    """The two-column cell on TUNE, IDLE and BOOST: a reading, the target
       beside it, and a bar across the bottom. Offsets are from the content
       box, so the cell's 8 px padding is added to each."""
    kids  = label(PAD+0, PAD+20, value, 36, TEXT)
    kids += label(PAD+104, PAD+30, tgt, 14, DIM)
    kids += bar(PAD+0, PAD+72, 288, 7, fill[0], fill[1], fill[2], marks)
    return tile(x,y,COL*2,ROW,caption,unit,"",28,sev=sev,children=kids)

boards={}

# ---------------- DRIVE / PARKED ---------------------------------------
def drive_page(rpm, rpm_fill, boost, mapkpa, boost_fill, clt, oil, lam, batt, oil_sev=None):
    p=[frame(0,0,290,156)+"</div>",
       label(12,12,"RPM",14,CAPTION),
       label(12,56,rpm,48,TEXT),
       bar(12,137,266,7,0,rpm_fill,CYAN),
       frame(290,0,190,156,f"border-left: 1px solid {LINE};")+"</div>",
       label(302,12,"BOOST",14,CAPTION),
       label(302,32,boost,36,TEXT),
       label(302,76,f"bar&nbsp; MAP {mapkpa} kPa",14,DIM),
       bar(302,94,162,10,min(74,74+boost_fill),abs(boost_fill),CYAN,marks=((74,1,DIM),(132,2,WARN))),
       label(302,114,"Peak +1.06 bar",14,DIM),
       tile(0,156,120,130,"CLT","C",clt,36),
       tile(120,156,120,130,"OIL P","bar",oil,36,sev=oil_sev),
       tile(240,156,120,130,"LAMBDA","",lam,36),
       tile(360,156,120,130,"BATT","V",batt,36)]
    return page_wrap(p)

boards["Drive.dc.html"]=doc("DRIVE",
    "".join(chrome(("DRIVE",0), latch="! 2", clock="14:32", lit=7))
    + drive_page("3450",150,"+0.84","184",50,"96","4.1","0.88","14.2"))

boards["Parked.dc.html"]=doc("PARKED",
    "".join(chrome(("DRIVE",0), link=("OFFLINE",CRIT), alarm="ENGINE OFF",
                   latch="! 2", wifi=WIFI+" 1", clock="14:52", lit=0))
    + drive_page("0",0,"+0.00","100",0,"88","0.0","1.00","12.4",oil_sev="crit"))

# ---------------- TUNE --------------------------------------------------
p=[hero(0,0,"AFR / TARGET","","14.2","tgt 14.7",(144,-30,WARN),((144,1,DIM),))]
p[0]=hero(0,0,"AFR / TARGET","","14.2","tgt 14.7",(114,30,GOOD),((144,1,DIM),))
p.append(tile(COL*2,0,COL,ROW,"KNOCK","V","1.2",36))
p.append(tile(0,ROW,COL,ROW,"IGN","BTDC","18.5",36))
p.append(tile(COL,ROW,COL,ROW,"INJ PW","ms","8.4",36))
p.append(tile(COL*2,ROW,COL,ROW,"INJ DC","%","46",36))
p.append(tile(0,ROW*2,COL,ROW,"MAP","kPa","184",36))
p.append(tile(COL,ROW*2,COL,ROW,"TPS","%","82",36))
p.append(tile(COL*2,ROW*2,COL,ROW,"RPM","rpm","3450",36))
boards["Tune.dc.html"]=doc("TUNE",
    "".join(chrome(("TUNE",1), latch="! 2", clock="14:33", lit=7)) + page_wrap(p))

# ---------------- TEMPS -------------------------------------------------
R=143
p=[tile(0,0,COL,R,"CLT","C","96",48),
   tile(COL,0,COL,R,"IAT","C","41",48),
   tile(COL*2,0,COL,R,"ECU T","C","38",48),
   tile(0,R,COL,R,"OIL P","bar","4.1",48),
   tile(COL,R,COL,R,"FUEL P","bar","3.2",48),
   tile(COL*2,R,COL,R,"DFPR","kPa","216",48)]
boards["Temps.dc.html"]=doc("TEMPS",
    "".join(chrome(("TEMPS",2), latch="! 2", clock="14:34", lit=5)) + page_wrap(p))

# ---------------- IDLE --------------------------------------------------
p=[hero(0,0,"RPM / IDLE TGT","","850","tgt 900",(0,96,GOOD),((104,2,CYAN),)),
   tile(COL*2,0,COL,ROW,"IDLE CTL","","CLOSED",28),
   tile(0,ROW,COL,ROW,"IDLE DC","%","38",36),
   tile(COL,ROW,COL,ROW,"IDLE PID","%","+2",36),
   tile(COL*2,ROW,COL,ROW,"AFR","","14.2",36),
   tile(0,ROW*2,COL,ROW,"IGN","BTDC","12.5",36),
   tile(COL,ROW*2,COL,ROW,"IDLE IGN","deg","+3.0",36),
   tile(COL*2,ROW*2,COL,ROW,"MAP","kPa","32",36)]
boards["Main.dc.html"]=doc("IDLE",
    "".join(chrome(("IDLE",3), clock="14:35", lit=1)) + page_wrap(p))

# ---------------- BOOST -------------------------------------------------
p=[hero(0,0,"MAP / BOOST TGT","kPa","224","tgt 220",(0,224,GOOD),((220,2,CYAN),)),
   tile(COL*2,0,COL,ROW,"BOOST SET","","1",36),
   tile(0,ROW,COL,ROW,"BOOST DC","%","62",36),
   tile(COL,ROW,COL,ROW,"BOOST PID","%","+4",36),
   tile(COL*2,ROW,COL,ROW,"ERR COR","%","-1",36),
   tile(0,ROW*2,COL,ROW,"RPM","rpm","5820",36),
   tile(COL,ROW*2,COL,ROW,"TPS","%","100",36),
   tile(COL*2,ROW*2,COL,ROW,"AFR","","11.8",36)]
boards["Boost.dc.html"]=doc("BOOST",
    "".join(chrome(("BOOST",4), alarm="KNOCK 2.4 V", alarm_color=WARN,
                   latch="! 1", clock="14:41", lit=13)) + page_wrap(p))

# ---------------- ALARMS ------------------------------------------------
HDR=22; RH=22; FTR=22; TT=4
cols=[(12,"TIME",None),(90,"SOURCE",None),(172,"VALUE",None),
      (240,"RPM",60),(316,"FOR",60),(408,"STATE",None)]
p=[frame(0,0,480,HDR,f"background: {BG}; border-bottom: 1px solid {LINE};")
   + "".join(label(x,TT,t,14,CAPTION,w=w,align="right" if w else "left") for x,t,w in cols)
   + "</div>"]
events=[("14:41:02","KNOCK","2.4 V","5820","2.1 s","ACT","warn",True),
        ("14:38:51","OIL P","1.9 bar","6100","0.6 s","RTN","crit",False),
        ("14:33:10","INJ DC","94 %","6240","1.4 s","RTN","warn",False),
        ("14:21:44","LINK","stale","1180","3.0 s","RTN","warn",False),
        ("14:08:19","CLT","103 C","2400","41 s","RTN","warn",False)]
for i,(t,src,val,rpm,dur,state,sev,active) in enumerate(events):
    y=HDR+i*RH
    stripe = CRIT if sev=="crit" else WARN
    body = TEXT if active else DIM
    accent = stripe if active else DIM
    p.append(frame(0,y,480,RH,f"border-bottom: 1px solid {TRACK};")
             + block(0,0,4,RH,stripe)
             + label(12,TT,t,14,body) + label(90,TT,src,14,accent,weight=600)
             + label(172,TT,val,14,body)
             + label(240,TT,rpm,14,DIM,w=60,align="right")
             + label(316,TT,dur,14,DIM,w=60,align="right")
             + label(408,TT,state,14,accent) + "</div>")
fy=286-FTR
p.append(frame(0,fy,480,FTR,f"background: {BG}; border-top: 1px solid {LINE};")
         + label(12,TT,"ACTIVE",14,CAPTION) + label(66,TT,"1",14,WARN)
         + label(92,TT,"TOTAL",14,CAPTION) + label(143,TT,"5",14,TEXT)
         + label(168,TT,"RUN",14,CAPTION) + label(208,TT,"42:11",14,TEXT)
         + label(258,TT,"",14,DIM)
         + label(300,TT,"SINCE 13:59:04",14,DOTOFF,w=168,align="right") + "</div>")
boards["Alarms.dc.html"]=doc("ALARMS",
    "".join(chrome(("ALARMS",5), alarm="KNOCK 2.4 V", alarm_color=WARN,
                   latch="! 5", clock="14:41", lit=13)) + page_wrap(p))

# ---------------- DIAG --------------------------------------------------
CW=240; RH2=17; P=10
def diag_row(x,y,name,value,vcolor=TEXT,width=CW-2*P):
    return label(x,y,name,14,CAPTION) + label(x,y,value,14,vcolor,w=width,align="right")
p=[frame(0,0,CW,286)+"</div>", frame(CW,0,CW,286,f"border-left: 1px solid {LINE};")+"</div>"]
L=P
p.append(label(L,P,"SERIAL LINK",14,CAPTION))
for i,(n,v) in enumerate([("State","ONLINE"),("Age","12 ms"),("Updates","482 119"),("Bad frames","0")]):
    p.append(diag_row(L,P+18+i*RH2,n,v))
p.append(label(L,P+94,"LATCHED THIS RUN",14,CAPTION))
for i,t in enumerate(["OIL P 1.9 bar @ 6100","KNOCK 2.4 V @ 5820",""]):
    p.append(label(L,P+112+i*14,t,14,DIM))
p.append(label(L,P+162,"CEL FLAGS",14,CAPTION))
p.append(diag_row(L,P+180,"Raw","0x0000"))
for i,n in enumerate(["CLT","IAT","MAP","WBO","EGT1","EGT2","EGT AL","KNOCK","FF SENS","DBW","FPR"]):
    p.append(label(L+(i%3)*72,P+200+(i//3)*16,n,14,DOTOFF))
R2=CW+P
p.append(label(R2,P,"PEAKS THIS RUN",14,CAPTION))
peaks=[("RPM max","6240"),("MAP max","224"),("CLT max","103"),("IAT max","44"),
       ("Oil P max","5.2"),("Oil P min","1.9"),("Fuel P max","3.4"),("Fuel P min","2.9"),
       ("Lambda min","0.78"),("Knock max","2.4"),("Inj DC max","94")]
for i,(n,v) in enumerate(peaks):
    p.append(diag_row(R2,P+18+i*RH2,n,v))
p.append(label(R2,P+208,"SERVICE AP",14,CAPTION))
p.append(diag_row(R2,P+226,"Network","EcuDash"))
p.append(diag_row(R2,P+243,"Key","carina1g"))
boards["Diag.dc.html"]=doc("DIAG",
    "".join(chrome(("DIAG",6), latch="! 5", clock="14:44")) + page_wrap(p))

# ---------------- SETUP -------------------------------------------------
STRIP=24; CATW=110; CATH=44; RW=480-CATW; SRH=32
p=[frame(0,0,480,STRIP,f"background: {BG}; border-bottom: 1px solid {LINE};")
   + midlabel(10,0,STRIP,"STORED IN FLASH",14,DIM)
   + midlabel(0,0,STRIP,f"{ARROW_UP} 5-12/18 {ARROW_DOWN}",14,DIM,w=480,align="center")
   + midlabel(0,0,STRIP,"ENGINE OFF",14,DIM,w=470,align="right") + "</div>"]
p.append(frame(0,STRIP,CATW,286-STRIP,f"border-right: 1px solid {LINE};")+"</div>")
cats=["Alarms","Limits","Shift","Display","Log"]
for i,name in enumerate(cats):
    y=STRIP+i*CATH
    sel = (name=="Limits")
    p.append(frame(0,y,CATW,CATH,f"border-bottom: 1px solid {LINE};")+"</div>")
    if sel: p.append(block(0,y,3,CATH,CYAN))
    p.append(midlabel(12,y,CATH,name,14,TEXT if sel else DIM))
rows=[("Lean warn","0.90",""),("Lean crit","0.85",""),("Batt warn","12.2","V"),
      ("Batt crit","11.8","V"),("Charge warn","14.8","V"),("Charge crit","15.2","V"),
      ("Knock warn","2.0","V"),("Knock crit","3.0","V")]
for i,(n,v,u) in enumerate(rows):
    y=STRIP+i*SRH
    p.append(frame(CATW,y,RW,SRH,f"border-bottom: 1px solid {LINE};")+"</div>")
    p.append(midlabel(CATW+10,y,SRH,n,14,CAPTION))
    p.append(midlabel(CATW+150,y,SRH,v,14,TEXT))
    p.append(midlabel(CATW+212,y,SRH,u,14,DIM))
    for bx,glyph in ((RW-90,"-"),(RW-46,"+")):
        p.append(frame(CATW+bx,y+4,36,SRH-9,f"background: {BG}; border: 1px solid {LINE};")
                 + label(0,(SRH-9-16)//2,glyph,14,TEXT,w=34,align="center") + "</div>")
boards["Setup.dc.html"]=doc("SETUP",
    "".join(chrome(("SETUP",7), link=("OFFLINE",CRIT), alarm="ENGINE OFF",
                   latch="! 5", wifi=WIFI, clock="14:53")) + page_wrap(p))

# ------------------------------------------------------------------------
# What each artboard is called in the canvas, and what its PNG is called here.
# "Main" is the canvas's entry board and cannot be renamed without the editor;
# it holds the IDLE page.
PNG_NAMES={
    "Drive.dc.html":"1-drive", "Tune.dc.html":"2-tune", "Temps.dc.html":"3-temps",
    "Main.dc.html":"4-idle",  "Boost.dc.html":"5-boost", "Alarms.dc.html":"6-alarms",
    "Diag.dc.html":"7-diag",  "Setup.dc.html":"8-setup", "Parked.dc.html":"drive-parked",
}

CHROME="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

here=os.path.dirname(os.path.abspath(__file__))
build=os.path.join(here,"build")
os.makedirs(build,exist_ok=True)

import re
for name,html in sorted(boards.items()):
    open(os.path.join(build,name),"w").write(html)

    # The same markup without the canvas wrapper, so a browser can render it.
    head=re.search(r"<helmet>(.*?)</helmet>", html, re.S).group(1)
    body=re.search(r"</helmet>\n(.*?)\n</x-dc>", html, re.S).group(1)
    page=("<!doctype html><html><head><meta charset=\"utf-8\">"+head+
          "<style>html,body{margin:0;padding:0;width:480px;height:320px;overflow:hidden}</style>"
          "</head><body>"+body+"</body></html>")
    standalone=os.path.join(build,PNG_NAMES[name]+".html")
    open(standalone,"w").write(page)

    if not os.path.exists(CHROME):
        continue
    # Two device pixels per panel pixel, so the PNG survives being looked at.
    subprocess.run([CHROME,"--headless=new","--disable-gpu","--hide-scrollbars",
                    "--force-device-scale-factor=2","--window-size=480,320",
                    "--screenshot="+os.path.join(here,PNG_NAMES[name]+".png"),
                    "file://"+standalone],
                   check=True, capture_output=True)

print("artboards ->", build)
if os.path.exists(CHROME):
    print("images    ->", here)
else:
    print("no Chrome at", CHROME, "- wrote the HTML only", file=sys.stderr)
