import struct
operation_dic = {
    'AND' : 0,
    'ORR' : 1,
    'EOR' : 2,
    'ADD' : 3,
    'ADC' : 4,
    'CMP' : 5,
    'SUB' : 6,
    'SBC' : 7,
    'MOV' : 8,
    'LSH' : 9,
    'RSH' : 10

}
branch_dic = {
    'B' : 8,
    'BEQ' : 9,
    'BNE' : 10,
    'BLE' : 11,
    'BGE' :12,
    'BL' : 13,
    'BG' : 14
 }

register_list = [['r'+str(i), i] for i in range (0,15)]


def check(op) :
    """check if operation is a register or not, make sure it's a existing register and return his decimal value """ 
    if op.isnumeric():
        return int(op)
    if '0x' in op:
        return int(op,16)
    for register in register_list :
        if op.lower() == register[0] :
            return register[1] # 1 for register
    print("invalid destination register or syntax ")
    return -1


def translator(iv,dest,op2,op1,op,ivf) :
        """Translate instruction like operation dest, op1, op2 into ordered binary code  """
        binary = 0
        binary = int(iv) << 0 | int(dest) << 8 | int(op2) << 12 |int(op1)<<16 | int(op) << 20 | int(ivf) << 24 
        return struct.pack('>I',binary)

def _branch(offset,bcc):
    """Translate branch instruction into ordered binary code"""
    if int(offset) >= 0 :
        signe = 0
    elif int(offset) < 0:
        offset = abs(int(offset))
        signe = 1
    binary = int(offset) << 0 | signe << 27 | bcc << 28 
    return struct.pack('>I',binary)


def interpreter(tab_instr) : 
    """write the compiled code in a file"""
    with open ('bin','wb') as f :
        for instr in tab_instr :
            if instr == 1:
                return 1
            if instr == 0 :
                continue
            instr_bin = instr_interpreter(instr)
            f.write(instr_bin)

def instr_interpreter(instr):
    """for every instruction : ordered the 32 bits instructions"""
    if instr == 0 : # check if it's null instruction line
        return 0   
    if instr[0].upper()  in operation_dic :        
        op,dest,op1,op2 = operation_dic[instr[0].upper()],check(instr[1]),instr[2],instr[3] 
        if op == 5 :
            if op2.isnumeric() or '0x' in op2:
                instr_bin =  translator(check(op2),0,0,check(op1),op,1)
            else: 
                instr_bin = translator(0,0,check(op2),check(op1),op,0)
        elif op == 8 :#mov
            if op2.isnumeric() or '0x' in op2:
                instr_bin =  translator(check(op2),dest,0,0,op,1)
            else: 
                instr_bin = translator(0,dest,check(op2),0,op,0)
        else : 
            if op2.isnumeric() or '0x' in op2:
                instr_bin =  translator(check(op2),dest,0,check(op1),op,1)
            else:
                instr_bin = translator(0,dest,check(op2),check(op1),op,0)
        return instr_bin
    elif  (instr[0].upper() in branch_dic):
            instr_bin = _branch(instr[1], branch_dic[instr[0].upper()])
            return instr_bin
    else : 
        return 1

       
      

