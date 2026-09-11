"""Rebuild the checked-in reproducible scenarios; no simulation is implemented here."""
from pathlib import Path
import json, copy
ROOT=Path(__file__).resolve().parents[1]
def save(name,x):
    (ROOT/'examples'/name).write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def item(type_,owner,place=None,holder=None,**kw):
    return dict(type=type_,owner=owner,placement=dict(kind='inventory' if holder else 'at',ref=holder or place),**kw)
def evidence(id_,subject,predicate,args,time=0):
    return dict(id=id_,source='scenario_genesis',roots=[id_],confidence=1,learned_at=time,statement=dict(subject=subject,predicate=predicate,arguments=args,valid_from=time))
def basic():
    places={p:dict(id=p,name=p) for p in ['shop','home','park']}
    edges={id_:dict(id=id_,**v) for id_,v in {'shop_home':dict(**{'from':'shop','to':'home'},minutes=5),'home_shop':dict(**{'from':'home','to':'shop'},minutes=5)}.items()}
    actors={p:dict(id=p,name=n,position=dict(place='shop'),beliefs=[evidence(f'{p}-{e}',e,'edge',dict(**{'from':v['from'],'to':v['to']},minutes=v['minutes'])) for e,v in edges.items()]) for p,n in [('a','Алексей'),('b','Борис'),('c','Игорь')]}
    items={'tool':item('I05','a',holder='a'),'kit':item('I06','a',holder='a'),'target':item('F03','a','shop'),'meal':item('I01','store','shop'),'phone_a':item('I10','a',holder='a'),'phone_b':item('I10','b',holder='b')}
    actors['a']['known_methods']=['R02','R01'];actors['a']['skills']={'repair':.6,'cooking':.3}
    return dict(places=places,edges=edges,actors=actors,items=items,accounts={'a':100,'b':60,'c':50,'store':1000},shops={'store':dict(id='store',place='shop',account='store',stock={'meal':30})})
def command(t,who,seq,type_,args):return dict(at=t,command=dict(actor=who,sequence=seq,type=type_,args=args))
w=basic()
script=[command(0,'a',1,'A14',dict(participants=['a','b'],expires=100,terms=dict(kind='joint',place='shop',start=0,end=100,minutes=20,topic='conversation'))),command(0,'c',1,'A06',dict(shop='store',item='meal')),command(1,'b',1,'A12',dict(proposal='a@1/proposal',version=1,answer='accept')),command(2,'a',2,'A15',dict(proposal='a@1/proposal',version=1)),command(2,'b',2,'A15',dict(proposal='a@1/proposal',version=1)),command(7,'b',3,'A22',dict(proposal='a@1/proposal')),command(8,'a',3,'A21',dict(recipe='R02',target='target',tools=['tool'],materials=['kit'])),command(8,'b',4,'A02',dict(edge='shop_home')),command(10,'c',2,'A07',dict(item='meal'))]
save('protocol.json',dict(description='Явные команды для проверки протоколов и сохранения посреди ремонта; не автономный сценарий.',world=w,script=script))
for accept in (True,False):
    w=basic();w['items']['spare_tool']=item('I05','store','shop');w['shops']['store']['stock']['spare_tool']=55
    w['actors']['b']['beliefs'] += [evidence('b-seen-tool','tool','item_seen',dict(type='I05',holder='a',place='shop')),evidence('b-store','store','shop_stock',dict(place='shop',offers=[dict(item='spare_tool',type='I05',price=55)]))]
    w['actors']['b']['outcome_counts']={'a/loan_item/accepted':8}
    w['actors']['b']['relations']={'a':dict(trust=.9)};w['actors']['a']['relations']={'b':dict(trust=.8 if accept else .05)}
    save('acquire_accept.json' if accept else 'acquire_refuse.json',dict(description='Ожидания Бориса одинаковы; меняется только отношение Алексея. Решение о согласии принимает контроллер Алексея.',world=w,controllers={'a':dict(mode='household'),'b':dict(mode='acquire',item_type='I05',due=240)}))

def household(count=6):
    now=360
    places={'shop':dict(id='shop',name='Магазин',open_windows=[[420,1320]]),'work':dict(id='work',name='Мастерская')}
    actors={};items={};edges={};accounts={'store':10000,'employer':100000};permissions={};contracts={}
    for i in range(count):
        who=f'n{i}';home=f'home{i}';places[home]=dict(id=home,name=f'Дом {i}',owner=who,private_space=True,public_entry=False)
        for frm,to in [(home,'shop'),('shop',home),(home,'work'),('work',home),('shop','work'),('work','shop')]:
            e=f'{frm}_{to}';edges[e]=dict(id=e,**{'from':frm,'to':to},minutes=8)
        accounts[who]=150
        for name,typ,holder,place in [(f'bed{i}','F01',None,home),(f'book{i}','I08',who,None),(f'phone{i}','I10',who,None),(f'workplace{i}','F04',None,'work')]:
            items[name]=item(typ,who,place,holder)
        contracts[who]=dict(id=who,worker=who,employer_account='employer',workplace=f'workplace{i}',rate_per_hour=37,next_payout=1080,payout_period=1440,windows=[[480,1020]])
        actors[who]=dict(id=who,name=f'Житель {i}',position=dict(place=home),horizon=120+i*40,needs={'N01':70,'N02':75,'N03':70,'N04':70},traits=dict(patience=.2+.12*i,caution=.3+.08*i,social_initiative=.3+.1*i),beliefs=[])
    offers=[]
    for i in range(120):
        it=f'meal_{i:03d}';items[it]=item('I01','store','shop');offers.append(dict(item=it,type='I01',price=30))
    for i,(who,a) in enumerate(actors.items()):
        a['beliefs']=[evidence(f'{who}-edge-{e}',e,'edge',dict(**{'from':v['from'],'to':v['to']},minutes=v['minutes']),now) for e,v in edges.items()]
        for key in (f'bed{i}',f'workplace{i}'):
            a['beliefs'].append(evidence(f'{who}-{key}',key,'item_seen',dict(type=items[key]['type'],place=items[key]['placement']['ref'],holder=''),now))
        a['beliefs'].append(evidence(f'{who}-stock','store','shop_stock',dict(place='shop',offers=offers,open_windows=[[420,1320]]),now))
    world=dict(time=now,seed='household-2026',places=places,edges=edges,actors=actors,items=items,accounts=accounts,contracts=contracts,permissions=permissions,shops={'store':dict(id='store',place='shop',account='store',slots=2,stock={x['item']:x['price'] for x in offers})})
    return dict(description='Бытовая политика шести NPC. 120 порций и фонд зарплаты заданы явно; скрытого пополнения нет.',world=world,controllers={a:dict(mode='household') for a in actors})
save('household.json',household())
# A small single actor snapshot for user editing, not a second simulation implementation.
w=basic();w['actors']['a']['needs']={'N01':30};w['items']['meal']['owner']='a';w['items']['meal']['placement']=dict(kind='inventory',ref='a');w['shops']['store']['stock']={}
save('food.json',dict(description='Один прерванный приём пищи: 7 + 8 минут.',world=w,script=[command(0,'a',1,'A07',dict(item='meal')),command(7,'a',2,'A22',{}),command(7,'a',3,'A07',dict(item='meal'))]))
print('Wrote 5 scenarios')
