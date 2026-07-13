/* sim.js — in-browser port of tools/simulator/simulate.py.
   Replaces WebSocket so the REAL onboard UI (index.html) runs with no rower:
   same CFG/METRICS/39-float-binary protocol, same Beta(2,3) stroke generator
   with injected technique faults. */
(() => {
const S = {tara:84213, scala:10412.5, h:181, w:75, pull:3, rel:1.5, ppr:600,
  circ:100, loff:0, ftp:200, ghost:[110,0,1.25,0.33], enc:true,
  watts:0, spm:0, pace:0, dist:0, kcal:0, strokes:0, hr:118, ticks:0,
  dropped:0, sync:0, pulling:false, force:0, cable:0, seat:0.35};
const g = s => (Math.random()*2-1)*s*1.2;
const B = u => u*(1-u)*(1-u);
const DEF = ["ok","ok","ok","ok","late_peak","spiky","double","shoot","slow_rec"];
function cfg(){const gh=S.ghost;
 return `CFG:${S.tara},${S.scala.toFixed(4)},${S.h.toFixed(1)},${S.w.toFixed(1)},${S.pull.toFixed(1)},${S.rel.toFixed(1)},${S.ppr.toFixed(1)},${S.circ.toFixed(1)},${S.loff.toFixed(1)},,,${S.ftp.toFixed(1)}|AA:BB:CC:DD:EE:FF|${gh[0].toFixed(2)},${gh[1].toFixed(2)},${gh[2].toFixed(2)},${gh[3].toFixed(2)}|${S.enc?1:0}`}
function ghostJson(){const [b2,,ld,dr]=S.ghost,F=[],VS=[],VA=[];let pk=0;
 for(let i=0;i<64;i++){const u=i/63,f=+(b2*Math.pow(B(u)/0.148,2)*0.35).toFixed(1);
  F.push(f);pk=Math.max(pk,f);
  VS.push(+((u<0.6?1.9*Math.sin(Math.min(1,u/0.6)*Math.PI):0)).toFixed(2));
  VA.push(+((u>0.55?1.6*Math.sin((u-0.55)/0.45*Math.PI):0)).toFixed(2))}
 const p=(20/60)*b2*Math.pow(ld,3)*0.35/Math.pow(dr*3,2);
 return `{"P_teorica":${p.toFixed(1)},"F_peak":${pk.toFixed(1)},"F_ghost":[${F}],"vs_ghost":[${VS}],"va_ghost":[${VA}]}`}
// ---- stroke engine (100 Hz) ----
let socks=[], cyc=null, phase=0, prev=0, triples=[], lastM=0, t=0;
function newCycle(){const spm=20.5+Math.random()*3.5,def=DEF[(Math.random()*DEF.length)|0];
 const T=60/spm,d=def==="slow_rec"?0.5:0.36;
 return{T,drv:d*T,def,peak:30+Math.random()*8,ld:1.28+Math.random()*0.12,work:0}}
cyc=newCycle();
function bc(m,bin){for(const s of socks)if(s.readyState===1&&s.onmessage)
 s.onmessage({data:bin?m.buffer.slice(0):m})}
function tick(){const dt=0.01;t+=dt;phase+=dt;
 if(phase>=cyc.T){phase-=cyc.T;S.strokes++;S.spm=Math.round(60/cyc.T);
  S.watts=Math.min(1500,Math.round(cyc.work/cyc.T));
  const v=S.watts>0?Math.pow(S.watts/2.8,1/3):0;S.pace=v>0?Math.round(500/v):0;
  S.kcal+=cyc.work/4184*4;cyc=newCycle()}
 const{drv,def,peak,ld}=cyc;let f,cable,seat;
 if(phase<drv){const u=phase/drv;
  let sh=def==="late_peak"?Math.pow(u,2.2)*(1-u)/0.105:
         def==="spiky"?Math.pow(B(u)/0.148,2.4):B(u)/0.148;
  f=peak*sh;
  if(def==="double")f*=1-0.38*Math.exp(-Math.pow((u-0.55)/0.09,2));
  f=Math.max(0,f+g(0.4));
  cable=ld*(1-Math.cos(u*Math.PI))/2;
  const us=Math.min(1,u/(def==="shoot"?0.25:0.75));
  seat=0.30+0.403*ld*(1-Math.cos(us*Math.PI))/2;
  const dc=cable-prev;
  if(dc>0){cyc.work+=f*9.81*dc;S.dist+=(S.watts?Math.pow(S.watts/2.8,1/3):2)*dt}
 }else{const u=(phase-drv)/(cyc.T-drv);
  f=Math.max(0,1.2*(1-u)+g(0.15));
  cable=ld*(1+Math.cos(u*Math.PI))/2;
  const us=Math.max(0,(u-0.25)/0.75);
  seat=0.30+0.403*ld*(1+Math.cos(us*Math.PI))/2}
 prev=cable;
 if(!S.enc)cable=0;
 S.force=f;S.cable=cable;S.seat=seat;S.pulling=phase<drv;
 S.ticks=cable>0?Math.round(cable/((S.circ*Math.PI/1000)/S.ppr)):0;
 S.hr+=(Math.min(178,112+S.watts*0.22+t*0.05)-S.hr)*0.002+g(0.1);
 triples.push(f,cable,seat);
 if(triples.length>=39){bc(new Float32Array(triples.slice(0,39)),true);S.sync++;triples=[]}
 if(t-lastM>=0.5){lastM=t;
  bc(`METRICS:${S.watts}|${Math.round(S.dist)}|${S.spm}|${S.pace}|${S.kcal.toFixed(1)}|1|${S.tara+Math.round(S.force*S.scala/100)}|${S.sync}|${Math.round(S.hr)}|${S.ticks}|${S.seat.toFixed(2)}|${S.force.toFixed(2)}|${S.pulling?1:0}|${S.cable.toFixed(3)}|${S.dropped}|0|${S.strokes}`)}}
setInterval(()=>{for(let i=0;i<3;i++)tick()},30);   // 100 Hz medi, batch da 3
// ---- fake WebSocket ----
class SimWS{
 constructor(){this.readyState=0;socks.push(this);
  setTimeout(()=>{this.readyState=1;this.onopen&&this.onopen({})},120)}
 send(txt){txt=String(txt).trim();
  if(txt==="GET_CFG")this._r(cfg());
  else if(txt==="GET_PBS")this._r("PBS:100=24,200=52,500=118,1000=245,2000=512,5000=1350,6000=0,10000=2820,21097=0,42195=0");
  else if(txt==="SCAN")setTimeout(()=>this._r("WIFI_LIST:CasaMia,FASTWEB-7A2F,Vodafone-A1B2"),300);
  else if(txt==="tare"||txt.startsWith("calib:"))bc(cfg());
  else if(txt.startsWith("mech:")){const[p,r]=txt.slice(5).split("|");
   S.pull=+p||S.pull;S.rel=+r||S.rel;bc(cfg())}
  else if(txt.startsWith("encmode:")){S.enc=txt.slice(8).trim()!=="0";bc(cfg())}
  else if(txt.startsWith("ghost:")){const v=txt.slice(6).split("|").map(Number);
   if(v.every(x=>!isNaN(x)))S.ghost=v;bc(ghostJson())}
  else if(txt.startsWith("CFG:")){const b=txt.slice(4).split(",");
   S.tara=Math.round(+b[0])||S.tara;S.scala=+b[1]||S.scala;S.h=+b[2]||S.h;S.w=+b[3]||S.w;
   S.pull=+b[4]||S.pull;S.rel=+b[5]||S.rel;S.ppr=+b[6]||S.ppr;S.circ=+b[7]||S.circ;
   S.loff=+b[8]||0;if(b.length>11)S.ftp=+b[11]||S.ftp;bc(cfg())}}
 _r(m){this.onmessage&&this.onmessage({data:m})}
 close(){this.readyState=3;socks=socks.filter(s=>s!==this)}}
SimWS.CONNECTING=0;SimWS.OPEN=1;SimWS.CLOSING=2;SimWS.CLOSED=3;
window.WebSocket=SimWS;
})();
