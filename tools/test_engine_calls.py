#!/usr/bin/env python3
"""Execute x86 bridge calls against ABI-checking stubs, without Wine or game mutation."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
# Expected native register/stack conventions, checked against pvzgame.exe disassembly.
SPECS=[
 ('pvzRoadWalk',0x52f2e0,[0x1000],{'eax':0x1000,'esi':0},[],0),
 ('pvzRoadAnimRate',0x52eff0,[0x1000,0x41400000],{'eax':0x1000},[0x41400000],4),
 ('pvzAddRoadVisual',0x40ddc0,[0x1000,2,7],{'eax':0x1000,'ebx':-3},[7,2],8),
 ('pvzRoadReanim',0x52c310,[0x1000],{'eax':0x1000},[],0),
 ('pvzDrawRoadZombie',0x52e2e0,[0x1000,0x2000],{'ebx':0x1000},[0x2000],4),
 ('pvzWalkIntoHouse',0x534780,[0x1000],{'eax':0x1000},[],0),
 ('pvzSunSound',0x432b00,[0x1000],{'edx':0x1000},[],0),
 ('pvzStartMower',0x458da0,[0x1000],{'esi':0x1000},[],0),
 ('pvzZombieDeathAnim',0x533240,[0x1000],{'eax':0x1000},[0],4),
 ('pvzZombieDamageStates',0x52a470,[0x1000],{'eax':0x1000,'edi':0},[],0),
 ('pvzSetButtonLabel',0x448470,[0x1000,0x2000],{'edx':0x2000},[0x1000],4),
 ('pvzDrawSeedPacketNoCost',0x4876f0,[0x1000,0x42c40000,0x40800000,60,100],{'ecx':100},[0x1000,0x42c40000,0x40800000,60,-1,0,0,0],0),
 ('pvzPreloadPlant',0x4681e0,[5],{'esi':5},[],0),
 ('pvzPreloadZombie',0x5369e0,[7],{'eax':7},[],0),
 ('pvzNewGame',0x44f890,[0x1000],{'eax':0x1000},[],0),
 ('pvzKillChooser',0x44fe70,[0x1000],{'esi':0x1000},[],0),
 ('pvzCloseChooser',0x486d20,[0x1000],{'ebx':0x1000},[],0),
 ('pvzCanPlant',0x40e020,[0x1000,3,2,5],{'eax':2},[0x1000,3,5],12),
 ('pvzSeedCost',0x467b00,[60],{'eax':60,'edx':-1},[],0),
 ('pvzAddCoin',0x40cb10,[0x1000,123,234,4,2],{'ecx':0x1000},[123,234,4,2],16),
 ('pvzCollectCoin',0x432060,[0x1000],{'ecx':0x1000},[],0),
 ('pvzMakeBoard',0x44f5f0,[0x1000],{'ecx':0x1000},[],0),
 ('pvzInitBoard',0x40af90,[0x1000],{},[0x1000],4),
 ('pvzStartPlaying',0x44f6b0,[0x1000],{'esi':0x1000},[],0),
 ('pvzKillSelector',0x44f9e0,[0x1000],{'esi':0x1000},[],0),
 ('pvzShowMenu',0x44f8e0,[0x1000],{'ecx':0x1000},[],0),
 ('pvzAddPlant',0x40d120,[0x1000,3,2,5],{'eax':2},[0x1000,3,5,-1],16),
 ('pvzAddZombie',0x40ddc0,[0x1000,2,4],{'eax':0x1000,'ebx':0},[4,2],8),
 ('pvzAddShot',0x40d620,[0x1000,123,234,2],{'eax':0x1000},[123,234,420000,2,0],20),
 ('pvzUpdateEffects',0x445890,[0x1000],{'edi':0x1000},[],0),
 ('pvzUpdateLayers',0x416080,[0x1000],{},[0x1000],4),
 ('pvzDrawSeedPacket',0x4876f0,[0x1000,0x42c40000,0x40800000,60,100],{'ecx':100},[0x1000,0x42c40000,0x40800000,60,-1,0,1,0],0),
 ('pvzDeleteQueue',0x41bad0,[0x1000],{'esi':0x1000},[],0),
 ('pvzPlantDie',0x4679b0,[0x1000],{},[0x1000],4),
 ('pvzZombieDie',0x530510,[0x1000],{'ecx':0x1000},[],0),
 ('pvzGridX',0x41c680,[0x1000,3,2],{'ecx':0x1000,'eax':3},[],0),
 ('pvzGridY',0x41c740,[0x1000,3,2],{'ebx':0x1000,'ecx':3,'eax':2},[],0),
]
MASK=0xffffffff
class CPU:
 def __init__(self,code,symbols):
  self.code,self.symbols=code,symbols;self.r=[0]*8;self.memory={};self.reads=[];self.pc=0;self.cf=self.zf=False;self.calls=[];self.returnvalues={}
 def byte(self):
  result=self.code[self.pc-0x100000];self.pc+=1;return result
 def imm(self,n=4,signed=False):
  value=sum(self.byte()<<(i*8) for i in range(n))
  return value-(1<<(8*n)) if signed and value&(1<<(8*n-1)) else value
 def load(self,address):
  self.reads.append(address);return self.memory.get(address,0)
 def push(self,value):
  self.r[4]=(self.r[4]-4)&MASK;self.memory[self.r[4]]=value&MASK
 def pop(self):
  value=self.load(self.r[4]);self.r[4]=(self.r[4]+4)&MASK;return value
 def operand(self,m):
  mode,rm=m>>6,m&7
  if mode==3:return ('r',rm)
  if rm==4:
   sib=self.byte();base=sib&7;index=(sib>>3)&7
   address=self.imm() if mode==0 and base==5 else self.r[base]
   if index!=4:address+=self.r[index]*(1<<(sib>>6))
  else:address=self.imm() if mode==0 and rm==5 else self.r[rm]
  if mode==1:address+=self.imm(1,True)
  if mode==2:address+=self.imm(4,True)
  return ('m',address&MASK)
 def get(self,operand):return self.r[operand[1]] if operand[0]=='r' else self.load(operand[1])
 def set(self,operand,value):
  if operand[0]=='r':self.r[operand[1]]=value&MASK
  else:self.memory[operand[1]]=value&MASK
 def compare(self,a,b):self.cf=(a&MASK)<(b&MASK);self.zf=(a&MASK)==(b&MASK)
 def run(self,symbol):
  self.pc=0x100000+self.symbols[symbol]
  for step in range(10000):
   if self.pc==0xdeadbeef:return
   if self.pc in self.targets:
    name,addr,args,regs,stack,cleanup=self.targets[self.pc]
    self.calls.append((self.r.copy(),[self.load(self.r[4]+4+i*4) for i in range(len(stack))]))
    names={'eax':0,'ecx':1,'edx':2,'ebx':3,'esp':4,'ebp':5,'esi':6,'edi':7}
    for reg,value in regs.items():assert self.r[names[reg]]==(value&MASK),(name,reg,self.r)
    for i,value in enumerate(stack):assert self.load(self.r[4]+4+i*4)==(value&MASK),(name,i,self.memory)
    self.pc=self.pop();self.r[4]+=cleanup
    self.r[0]=self.returnvalues.get(addr,0x1234);self.r[1]=0xabc;self.r[2]=0xdef;continue
   opcode=self.byte()
   if 0x50<=opcode<=0x57:self.push(self.r[opcode-0x50])
   elif 0x58<=opcode<=0x5f:self.r[opcode-0x58]=self.pop()
   elif 0xb8<=opcode<=0xbf:self.r[opcode-0xb8]=self.imm()
   elif opcode==0x6a:self.push(self.imm(1,True))
   elif opcode==0x68:self.push(self.imm())
   elif opcode in (0x8b,0x89,0x8d,0x69,0x85,0x29):
    m=self.byte();reg=(m>>3)&7;operand=self.operand(m)
    if opcode==0x8b:self.r[reg]=self.get(operand)
    elif opcode==0x89:self.set(operand,self.r[reg])
    elif opcode==0x8d:self.r[reg]=operand[1]
    elif opcode==0x69:self.r[reg]=(self.get(operand)*self.imm())&MASK
    elif opcode==0x29:self.set(operand,self.get(operand)-self.r[reg])
    else:self.zf=(self.get(operand)&self.r[reg])==0
   elif opcode in (0x81,0x83):
    m=self.byte();operation=(m>>3)&7;operand=self.operand(m);value=self.imm(4 if opcode==0x81 else 1,opcode==0x83)
    if operation==0:self.set(operand,self.get(operand)+value)
    elif operation==5:self.set(operand,self.get(operand)-value)
    elif operation==7:self.compare(self.get(operand),value)
    else:raise AssertionError(('unsupported arithmetic',operation))
   elif opcode==0x3d:self.compare(self.r[0],self.imm())
   elif opcode==0x2d:self.r[0]=(self.r[0]-self.imm())&MASK
   elif opcode==0xe9:
    relative=self.imm(4,True);self.pc+=relative
   elif opcode==0xe8:
    relative=self.imm(4,True);self.push(self.pc);self.pc+=relative
   elif opcode in (0x72,0x77,0x75,0x74):
    relative=self.imm(1,True)
    if (opcode==0x72 and self.cf) or (opcode==0x77 and not self.cf and not self.zf) or (opcode==0x75 and not self.zf) or (opcode==0x74 and self.zf):self.pc+=relative
   elif opcode==0xff:
    m=self.byte();operation=(m>>3)&7;operand=self.operand(m);value=self.get(operand)
    if operation==6:self.push(value)
    elif operation in (2,4):
     if operation==2:self.push(self.pc)
     self.pc=value
    else:raise AssertionError(('unsupported FF',operation))
   elif opcode in (0xc3,0xc2):
    adjustment=self.imm(2) if opcode==0xc2 else 0;self.pc=self.pop();self.r[4]+=adjustment
   else:raise AssertionError(('unsupported instruction',hex(opcode),hex(self.pc-1)))
  raise AssertionError('bridge did not return')
def main():
 import struct
 with tempfile.TemporaryDirectory(prefix='pvzmp-abi-') as directory:
  obj=Path(directory)/'bridge.obj'
  subprocess.run(['clang','--target=i686-windows-gnu','-c',str(ROOT/'multiplayer/hook/engine-calls.S'),'-o',str(obj)],check=True)
  b=obj.read_bytes();count=struct.unpack_from('<H',b,2)[0];sympos,syms=struct.unpack_from('<II',b,8)
  strings=sympos+syms*18;symbols={}
  for i in range(count):
   at=20+i*40
   if b[at:at+8].rstrip(bytes([0]))==b'.text':
    size,raw=struct.unpack_from('<II',b,at+16);code=b[raw:raw+size]
  for i in range(syms):
   at=sympos+i*18;raw=b[at:at+8]
   if raw[:4]==bytes(4):start=strings+struct.unpack_from('<I',raw,4)[0];name=b[start:b.find(bytes([0]),start)].decode(errors='replace')
   else:name=raw.rstrip(bytes([0])).decode(errors='replace')
   symbols[name]=struct.unpack_from('<I',b,at+8)[0]
  for name,addr,args,regs,stack,cleanup in SPECS:
   cpu=CPU(code,symbols);cpu.targets={addr:(name,addr,args,regs,stack,cleanup)};cpu.r[4]=0x900000
   cpu.r[3]=0x2222;cpu.r[6]=0x3333;cpu.r[7]=0x4444;cpu.r[5]=0x5555
   for arg in reversed(args):cpu.push(arg)
   cpu.push(0xdeadbeef);cpu.run('_'+name)
   assert cpu.r[4]==0x900000-4*len(args),(name,'stack balance')
   assert (cpu.r[3],cpu.r[6],cpu.r[7],cpu.r[5])==(0x2222,0x3333,0x4444,0x5555),(name,'nonvolatile registers')
  # Resolve the external gate predicate as the linker would; verify both LAN
  # suppression and vanilla forwarding, including original graphics register.
  gated=bytearray(code);gate=symbols['_pvzBottomGate'];call=gate+bytes(gated[gate:gate+20]).index(b'\xe8')
  struct.pack_into('<i',gated,call+1,0x500000-(0x100000+call+5))
  for active in (0,1):
   cpu=CPU(gated,symbols);cpu.r[4]=0x900000;cpu.r[7]=0x4444
   cpu.targets={0x500000:('predicate',0x500000,[],{},[0x1000],0),0x41a0a0:('nativeUI',0x41a0a0,[],{'edi':0x4444},[0x1000],4)}
   cpu.returnvalues[0x500000]=active;cpu.push(0x1000);cpu.push(0xdeadbeef);cpu.run('_pvzBottomGate')
   assert cpu.r[4]==0x900000 and cpu.r[7]==0x4444
   assert len(cpu.calls)==(1 if active else 2),'native HUD is skipped only during LAN'
  print('Native UI call gate OK: LAN suppression and vanilla forwarding.')
  # The chooser gates use several non-standard native ABIs. Resolve each
  # external helper and test LAN and ordinary-game paths against native stubs.
  def gate_code(symbol,target):
   gated=bytearray(code);start=symbols['_'+symbol];call=start+bytes(gated[start:start+24]).index(b'\xe8')
   struct.pack_into('<i',gated,call+1,target-(0x100000+call+5));return gated
  gate_specs=[
   ('pvzSeedAvailableGate',0x500010,[],[5],0x453b20,[],{'ebx':5,'edi':0x1000},-1,1),
   ('pvzSeedCountGate',0x500020,[],[],0x453ac0,[],{'eax':0x1000},-1,49),
   ('pvzChooserCardGate',0x500030,[0x2000,0x42c40000,0x40800000,3,-1,0,1,0],[3],0x4876f0,[0x2000,0x42c40000,0x40800000,63,-1,0,1,0],{'ecx':255},63,-1),
   ('pvzChooserStartGate',0x500040,[],[0x1000],0x485600,[],{'eax':0x1000},0,1),
  ]
  for name,helper,args,hargs,native_target,native_stack,native_regs,forward,suppress in gate_specs:
   for answer in (forward,suppress):
    cpu=CPU(gate_code(name,helper),symbols);cpu.r[4]=0x900000;cpu.r[0]=0x1000;cpu.r[1]=255;cpu.r[2]=0x4444;cpu.r[3]=5;cpu.r[7]=0x1000
    cpu.targets={helper:('helper',helper,[],{},hargs,0),native_target:('native',native_target,[],native_regs,native_stack,0)}
    cpu.returnvalues[helper]=answer
    for arg in reversed(args):cpu.push(arg)
    cpu.push(0xdeadbeef);cpu.run('_'+name)
    assert len(cpu.calls)==(2 if answer==forward else 1),(name,'conditional native dispatch')
    assert cpu.r[4]==0x900000-4*len(args),(name,'caller arguments preserved')
    assert cpu.r[3]==5 and cpu.r[7]==0x1000,(name,'nonvolatile registers')
  print('Native chooser gates OK: availability, card mapping, seed count, start, and vanilla forwarding.')
  custom=gate_code('pvzChooserCardGate',0x500030)
  start=symbols['_pvzCustomCardGate'];call=start+bytes(custom[start:start+80]).index(b'\xe8')
  struct.pack_into('<i',custom,call+1,0x500050-(0x100000+call+5))
  cpu=CPU(custom,symbols);cpu.r[4]=0x900000;cpu.r[1]=123
  args=[0x2000,0x42c40000,0x40800000,3,-1,0,1,0]
  cpu.targets={0x500030:('map',0x500030,[],{},[3],0),0x500050:('customCard',0x500050,[],{},args[:3]+[122]+args[4:]+[123],0)}
  cpu.returnvalues[0x500030]=122
  for value in reversed(args):cpu.push(value)
  cpu.push(0xdeadbeef);cpu.run('_pvzChooserCardGate')
  assert len(cpu.calls)==2 and cpu.r[4]==0x900000-32
  death=gate_code('pvzZombieDeathGate',0x500060)
  cpu=CPU(death,symbols);cpu.r[4]=0x900000;cpu.r[0]=0x2000;cpu.r[1]=123;cpu.r[2]=456
  cpu.targets={0x500060:('deathAuthority',0x500060,[],{},[0x2000],0)};cpu.returnvalues[0x500060]=0
  cpu.push(0);cpu.push(0xdeadbeef);cpu.run('_pvzZombieDeathGate')
  assert cpu.r[4]==0x900000 and cpu.r[:3]==[0x2000,123,456]
  print('Custom card grayness/stack ABI and deferred predicted death ABI OK.')
  # Regression for the swapped row/column and missing zombie row: exercise all
  # 45 lawn cells, then execute the actual field-writing initializer bytes.
  game=ROOT/'pvzgame.exe'
  if game.exists():
   exe=game.read_bytes();pe=struct.unpack_from('<I',exe,0x3c)[0]
   sections=pe+24+struct.unpack_from('<H',exe,pe+20)[0]
   def native(address,length):
    rva=address-0x400000
    for i in range(struct.unpack_from('<H',exe,pe+6)[0]):
     section=sections+40*i;size,va,rawsize,raw=struct.unpack_from('<IIII',exe,section+8)
     if va<=rva<va+max(size,rawsize):return exe[raw+rva-va:raw+rva-va+length]
    raise AssertionError('unmapped native address')
   import re
   layout=(ROOT/'multiplayer/hook/native-layout.h').read_text()
   stride=int(re.search(r'CoinStride=(0x[0-9a-f]+)',layout)[1],16)
   offset=int(re.search(r'CoinID=(0x[0-9a-f]+)',layout)[1],16)
   assert native(0x41e06e,6)==b'\x69\xf6'+struct.pack('<I',stride),'coin allocator stride'
   assert native(0x41e08c,6)==b'\x89\x86'+struct.pack('<I',offset),'coin allocator ID store (not tail padding)'
   assert native(0x41718f,5)==b'\xe8'+struct.pack('<i',0x431810-0x417194),'verified native coin render call'
   assert native(0x431f27,3)==bytes.fromhex('c20800'),'coin rendering consumes two stack arguments'
   print('Native sun layout/render ABI OK: ID offset agrees with the actual allocator store.')
   assert native(0x533240,5)==bytes.fromhex('83ec085355'),'whole relocated death prologue'
   assert native(0x533591,3)==bytes.fromhex('c20400'),'death consumes damage flags'
   assert native(0x4661bd,5)==b'\xe8'+struct.pack('<i',0x42a040-0x4661c2),'scoped portrait call'
   plant_init=native(0x45db74,10)
   assert plant_init==bytes.fromhex('8bf08bf9897b2889731c')
   zombie_init=native(0x52258b,2)+native(0x52259c,3)
   assert zombie_init==bytes.fromhex('8bf089771c')
   assert native(0x40de18,3)==bytes.fromhex('8b450c'),'native zombie row comes from stack arg 2'
   assert native(0x48821b,1)==b'\xc3','seed card arguments are caller-cleaned'
   for row in range(5):
    for col in range(9):
     for zombie in (False,True):
      spec=('pvzAddZombie',0x40ddc0,[0x1000,row,4],{'eax':0x1000,'ebx':0},[4,row],8) if zombie else ('pvzAddPlant',0x40d120,[0x1000,col,row,5],{'eax':row},[0x1000,col,5,-1],16)
      cpu=CPU(code,symbols);cpu.targets={spec[1]:spec};cpu.r[4]=0x900000
      for arg in reversed(spec[2]):cpu.push(arg)
      cpu.push(0xdeadbeef);cpu.run('_'+spec[0]);registers,arguments=cpu.calls[0]
      init=CPU((zombie_init if zombie else plant_init)+b'\xc3',{'init':0});init.targets={};init.r[4]=0x900000
      init.r[0]=arguments[1] if zombie else registers[0];init.r[1]=0 if zombie else arguments[1]
      init.r[7 if zombie else 3]=0x2000;init.push(0xdeadbeef);init.run('init')
      assert init.memory[0x201c]==row
      if not zombie:assert init.memory[0x2028]==col
   print('Native initializer bytes OK: row/column storage for all 45 cells and all zombie lanes.')
  for size in (0,1,4095,4096,4097,20000,220000):
   cpu=CPU(code,symbols);cpu.targets={};cpu.r[4]=0x900000;cpu.r[0]=size;cpu.r[1]=0x3344;cpu.r[2]=0x5566;cpu.push(0xdeadbeef);cpu.run('__alloca')
   assert cpu.r[4]==0x900000-size and cpu.r[1]==0x3344 and cpu.r[2]==0x5566,'stack allocator ABI'
   expected=[0x900000-offset for offset in range(4096,size,4096)]+[0x900000-size]
   assert all(address in cpu.reads for address in expected),'all guard pages probed'
  print('Emulated x86 ABI OK: compiled bytes, native calls, argument order, preserved registers, stack balance and guard-page probes.')
if __name__=='__main__':main()
