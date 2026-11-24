import pandas as pd,numpy as np,math
f=pd.read_csv('datos_con_perturbacion_potenciometro_20261103.csv')
t=f.Tiempo_s.values;th=f.Posicion_rad.values;w=f.Velocidad_rad_s.values;u=f.Control_u.values;d=f.Perturbacion.values
print("--- perfil de d(t) (cada 5 s): t, d, theta[mrad], u")
for k in range(0,len(t),500): print(f"{t[k]:6.1f} d={d[k]:+.3f} th={th[k]*1e3:+7.1f} u={u[k]:+.3f}")
# trozos casi constantes de d (>=3 s con |dd|<0.01)
segs=[];i=0
while i<len(t):
    j=i
    while j+1<len(t) and abs(d[j+1]-d[i])<0.01: j+=1
    if t[j]-t[i]>=3.0: segs.append((i,j))
    i=j+1
print("\n--- tramos con d casi constante (>=3 s): d, media theta ultimos 1.5 s, media u ultimos 1.5 s, u+d")
for i,j in segs:
    k=slice(max(i,j-150),j+1)
    print(f"t={t[i]:5.1f}-{t[j]:5.1f}  d={d[k].mean():+.3f}  th={th[k].mean()*1e3:+6.2f} mrad (sd {th[k].std()*1e3:.2f})  u={u[k].mean():+.3f}  u+d={u[k].mean()+d[k].mean():+.4f}")
print("\nextremos theta:",th.min()*1e3,th.max()*1e3,"mrad; max|u|=",abs(u).max(), "; max|w|=",abs(w).max())
# simulacion con el d registrado
g=9.81;L=0.3;m=0.1;bf=0.005;J=m*L*L;invJ=1/J;gL=g/L;Ts=1e-5;LSB=math.pi/4095
KP,KI,KD=5.0,2.0,0.15
x=0.1;v=0.0;I=0.0;prev=0.1;wf=0.0;cnt=0
sim=[]
for n in range(len(t)):
    dd=d[n]
    for s in range(1000):
        meas=round(x/LSB)*LSB
        e=-meas
        I=max(-2,min(2,I+e*Ts)); cnt+=1
        if cnt>=1000:
            wm=(meas-prev)/0.01;prev=meas;cnt=0;wf=0.614*wf+0.386*wm
        uu=max(-10,min(10,KP*e-KD*wf+KI*I))
        v+=Ts*(gL*x-bf*invJ*v+invJ*(uu+dd));x+=Ts*v
    sim.append((x,wf,uu))
sim=np.array(sim)
r=lambda a,b:math.sqrt(np.mean((a-b)**2))
print(f"\nRMS DSP vs modelo (con el d real): theta {r(th,sim[:,0])*1e3:.2f} mrad, vel {r(w,sim[:,1]):.3f}, u {r(u,sim[:,2]):.3f}")
print(f"RMS solo t>=2 s: theta {r(th[200:],sim[200:,0])*1e3:.2f} mrad ; vs. std(theta)={th[200:].std()*1e3:.2f} mrad")
print(f"picos: DSP {th.max()*1e3:.1f}/{th.min()*1e3:.1f}  modelo {sim[:,0].max()*1e3:.1f}/{sim[:,0].min()*1e3:.1f} mrad")
np.save('sim_pert_out.npy',sim)
