def get_code(filename):
    """arg : (string) : file name
       return : (list) : list of all instruction in the code file """
    file_content = []
    with open (filename,'r') as f :
        lines = f.readlines()
        for line in lines :
            file_content.append(line) 
    return file_content
    
def cleaning_space(string_inst):
    """arg : instruction line
        return : (string) instruction line cleared of space and tab before and after """
    while string_inst[0] == ' ' or string_inst[0]== "\t":
        string_inst = string_inst[1:]
    while string_inst[-1] == ' ' or string_inst[-1] == '\t':
        string_inst = string_inst[:-1]
    return string_inst

def cleaning_noise(inst_list):
    """arg : (list) : list of instruction
        do : clean noise 
       return : (list) : cleaned list of instruction (without \n,\t,space,empty line etc)"""
    cleaned_instr = []
    for inst in inst_list :
        if inst != ' \n' or inst != '\n':
            inst = cleaning_space(inst)
            com = inst.find(";")
            rc = inst.find('\n')
            if inst.find(";") != -1 :
                s_com = inst.index(';')
                inst = inst[:s_com]
                cleaned_instr.append(inst)
            elif inst.find("\n") != -1 :
                s_n = inst.index('\n')
                inst = inst[:s_n]
                cleaned_instr.append(inst)
            else:
                cleaned_instr.append(inst)
    return cleaned_instr

def verif_op(instr):
    """arg: (string) :cleaned instruction 
       do : verified syntax of instruct 
       return :(list) : [type ,instruction] 
        """
    if instr =='':
        return 0
    find_type = instr.find(',')
    if find_type != -1 : 
        authorized_op = ['AND','ADD','CMP','MOV','ORR','EOR','ADC','SUB','SBC','LSH','RSH']
        temp = instr.split(',')
        temp2 = temp[0].split(' ')
        op = temp2[0]
        if op.upper() not in authorized_op :
            print("not known operation, syntax error")
            return 1
        if len(temp) > 2 : 
            op,dest,op1,op2 = temp2[0],temp2[1],temp[1],temp[2] # si de la forme OP ,dest op1,op2
        elif op.upper() =='CMP' :
            op,dest,op1,op2 = temp2[0],'0',temp2[1],temp[1] #si comparaison ==> dest= 0
        else : 
            op,dest,op1,op2 = temp2[0],temp2[1],'0',temp[1] #si mov ==> op1 = 0 Mov dest, op2                
       
        dest,op1,op2 = cleaning_space(dest), cleaning_space(op1),cleaning_space(op2)
        return [op,dest,op1,op2]
    else : 
        authorized_branch = ['B','BEQ','BNE','BLE','BGE','BL','BG']
        for branch in authorized_branch : 
            if instr.find(branch) != -1 :
                return instr.split(' ') 
        return 1 

def parser(filename):
    parsed_code = []
    asm_clean = cleaning_noise(get_code(filename))
    for inst in asm_clean :
        parsed_instr = verif_op(inst)
        print(parsed_instr)
        if parsed_instr == 1 :
            return 1
        if parsed_instr == 0 :
            pass
        parsed_code.append(parsed_instr)
    return parsed_code

