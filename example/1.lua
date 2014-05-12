symtable = {}
node = {}
triple = {}

final_code = nil

backpatchlist = {}

block_entrance = {}

strings = {}

label_num = 0

--from https://gist.github.com/rangercyh
function print_lua_table (lua_table, indent)
indent = indent or 0
for k, v in pairs(lua_table) do
if type(k) == "string" then
k = string.format("%q", k)
end
local szSuffix = ""
if type(v) == "table" then
szSuffix = "{"
end
local szPrefix = string.rep(" ", indent)
formatting = szPrefix.."["..k.."]".." = "..szSuffix
if type(v) == "table" then
print(formatting)
print_lua_table(v, indent + 1)
print(szPrefix.."},")
else
local szValue = ""
if type(v) == "string" then
szValue = string.format("%q", v)
else
szValue = tostring(v)
end
print(formatting..szValue..",")
end
end
end


function helper_dump_symtable()
	print "Symbol Table:"
	print_lua_table(symtable, 0)
end

function helper_dump_node()
	print "Node Table:"
	print_lua_table(node, 0)
end

function helper_dump_triple()
	print "Triple Table:"
	print_lua_table(triple, 0)
end

function print_triple(i, tri)
		t2 = tri[2]
		t3 = tri[3]
		if type(t2) == "table" then
			if t2.name ~= nil then
				t2 = t2.name
			else
				t2 = 't'
			end
		elseif type(t2) == "nil" then
			t2 = ""
		end
		
		if type(t3) == "table" then
			if t3.name ~= nil then
				t3 = t3.name
			else
				t3 = 't'
			end
		elseif type(t3) == "nil" then
			t3 = ""
		end
		
		print (i .. " " .. tri[1] .. " " .. t2 .. " " .. t3)
end

function helper_print_code()
	local t2 = nil
	local t3 = nil
	for i, v in pairs(final_code) do
		print_triple(i, triple[v])
	end
end

function helper_gencode(op, x1, x2)
	local i = #triple
	local newtriple = {op, x1, x2}
	
	triple[i+1] = newtriple
	return i+1
end

function helper_genlabel()
	local i = #triple
	local newtriple = {'L', label_num}
	label_num = label_num + 1
	triple[i+1] = newtriple
	return i+1, label_num-1
end

function helper_concatcode(...)
	local newtable = {}
	
	for j, k in pairs{...} do
		if k ~= nil then
			for i = 1, #k do
				table.insert(newtable, k[i])
			end
		end
	end
	return newtable
end

function helper_lookupsym(symname)
	for j, k in pairs(symtable) do
		if(k.name == symname) then
			return j
		end
	end
	return -1
end

function helper_checkdefined(val)
	if(node[val].addr ~= nil and node[val].addr.type == nil) then
		print ("Grammar Error: " .. node[val].addr.name .. " not defined.")
		return false
	end
	
	return true
end

function helper_patchback(table, dest)
	for j, k in pairs(table) do
		k[3] = dest
	end
end

-----------------------------------------------------
function process_int()
	local i = #node
	local newrecord = {type="int"}
	
	node[i+1] = newrecord
	return i+1
end

function process_helper_directcopy()
	--local i = #node
	local val = parser_getstackcontent(-1)
	--local newrecord = {}
	
	--newrecord=node[val]
	--node[i+1] = newrecord
	--return i+1
	return val
end

function process_identifier()
	local symi = #symtable
	local nodei = #node
	local newsym = nil
	local newnode = {}
	local tmp = parser_getcurrenttokenname()
	local tmp2 = helper_lookupsym(tmp)
	
	
	if(tmp2 == -1) then
		newsym = {}
		newsym.name = tmp
		symtable[symi+1] = newsym
	else
		newsym = symtable[tmp2]
	end
	
	newnode.addr = newsym
	newnode.val = newsym
	node[nodei+1] = newnode
	return nodei+1
end

function process_string()
	local i = #node
	local newrecord = {}
	local str = parser_getcurrenttokenname()
	table.insert(strings, str)
	newrecord.val = #strings
	newrecord.type = "string"
	node[i+1] = newrecord
	return i+1
end

function process_declspec_initdecllist_sc()
	--local nodei = #node
	--local newnode = {}
	local typeval = parser_getstackcontent(-3)
	local declval = parser_getstackcontent(-2)
	

	for i = 1, #node[declval].addrlist do
		if(node[declval].addrlist[i].type == "array") then
			node[declval].addrlist[i].arrtype = node[typeval].type
		else
			node[declval].addrlist[i].type = node[typeval].type
		end
	end

	--newnode.addrlist = node[declval].addrlist
	--newnode.code = node[declval].code
	
	--node[nodei+1] = newnode
	--return nodei+1
	return declval
end

function process_initdecllist_comma_initdecl()
	local listval = parser_getstackcontent(-3)
	local val = parser_getstackcontent(-1)
	
	table.insert(node[listval].addrlist, node[val].addr)
	node[listval].code = helper_concatcode(node[listval].code, node[val].code)
	return listval
end

function process_argument()
	local val = parser_getstackcontent(-1)
	
	if(node[val].type == "string") then
		node[val].val = "$__STRING_" .. node[val].val
	end
	node[val].arglist = {node[val].val}
	
	return val
end

function process_argexprlist_comma_assignexpr()
	local listval = parser_getstackcontent(-3)
	local val = parser_getstackcontent(-1)
	
	node[listval].code = helper_concatcode(node[listval].code, node[val].code)
	table.insert(node[listval].arglist, node[val].val);
	return listval
end

function process_initdecl()
	local val = parser_getstackcontent(-1)
	
	node[val].addrlist = {node[val].addr}
	node[val].addr = nil
	return val
end

function process_number()
	local i = #node
	local newrecord = {}
	newrecord.val = tonumber(parser_getcurrenttokenname())
	node[i+1] = newrecord
	return i+1
end

function process_decl_assign_init()
	local tar = parser_getstackcontent(-3)
	local val = parser_getstackcontent(-1)
	local tmp = nil

	if(node[tar].addr.type == "array") then
		node[tar].addr.vallist = node[val].vallist
		node[tar].code = {}
		for i = 1, node[tar].addr.arrnum do
			tmp = helper_gencode('A', node[tar].addr, i-1)
			table.insert(node[tar].code, tmp)
			table.insert(node[tar].code, helper_gencode('=', triple[tmp], node[tar].addr.vallist[i]))
			node.val = triple[tmp]
		end
	else
		node[tar].val = node[val].val
		node[tar].code = helper_concatcode(node[tar].code, node[val].code, {helper_gencode('=', node[tar].addr, node[val].val)})
	end
	
	return tar
end

function process_declaration()
	--helper_dump_triple()
	--print(parser_getstackcontent(-1))
	return process_helper_directcopy()
end

function process_statement()
	--helper_dump_triple()
	--print_lua_table(node[parser_getstackcontent(-1)].code)
	return process_helper_directcopy()
end

function process_valid_parse_structure()
	local val1 = parser_getstackcontent(-2)
	local val2 = parser_getstackcontent(-1)
	
--[[helper_dump_symtable()
	helper_dump_node()
	helper_dump_triple()
--]]
	node[val1].code = helper_concatcode(node[val1].code, node[val2].code)
	--helper_dump_triple()
	--print_lua_table(node[val1].code)
	return val1	
end

function process_directdecl_lb_assignexpr_rb()
	local val = parser_getstackcontent(-4)
	local val2 = parser_getstackcontent(-2)
	
	node[val].addr.type = "array"
	node[val].addr.arrnum = tonumber(node[val2].val)
	
	return val
end

function process_init()
	local val = parser_getstackcontent(-1)
	
	node[val].vallist = {node[val].val}
	node[val].val = nil
	
	return val
end

function process_initlist_comma_init()
	local listval = parser_getstackcontent(-3)
	local val = parser_getstackcontent(-1)
	
	table.insert(node[listval].vallist, node[val].val)
	
	return listval
end

function process_lb_initlist_rb()
	local val = parser_getstackcontent(-2)
	return val
end

function process_expr_sc()
	local val = parser_getstackcontent(-2)
	return val
end

function process_lb_initlist_comma_rb()
	local val = parser_getstackcontent(-3)
	return val
end

function process_lb_blockitemlist_rb()
	local val = parser_getstackcontent(-2)
	return val
end

function process_directdecl_lp_rp()
	local val = parser_getstackcontent(-3)
	return val
end

function process_postfixexpr_assign_assignexpr()
	local tar = parser_getstackcontent(-3)
	local val = parser_getstackcontent(-1)
	
	if(helper_checkdefined(val) == false or helper_checkdefined(tar) == false) then
		return -1
	end
	
	node[tar].val = node[val].val
	node[tar].code = helper_concatcode(node[tar].code, node[val].code, {helper_gencode('=', node[tar].addr, node[val].val)})
	
	print_lua_table(node[tar].code)
	
	return tar
end

function process_multiexpr_star_postfixexpr()
	local val = parser_getstackcontent(-3)
	local val2 = parser_getstackcontent(-1)
	if(helper_checkdefined(val) == false or helper_checkdefined(val2) == false) then
		return -1
	end
	
	local tmp = helper_gencode('*', node[val].val, node[val2].val)
	node[val].val = triple[tmp]
	node[val].code = helper_concatcode(node[val].code, node[val2].code, {tmp})
	
	return val
end


function process_multiexpr_slash_postfixexpr()
	local val = parser_getstackcontent(-3)
	local val2 = parser_getstackcontent(-1)

	if(helper_checkdefined(val) == false or helper_checkdefined(val2) == false) then
		return -1
	end
	
	local tmp = helper_gencode('/', node[val].val, node[val2].val)
	node[val].val = triple[tmp]
	node[val].code = helper_concatcode(node[val].code, node[val2].code, {tmp})
	
	return val
end

function process_addiexpr_plus_multiexpr()
	local val = parser_getstackcontent(-3)
	local val2 = parser_getstackcontent(-1)
	local tmp = helper_gencode('+', node[val].val, node[val2].val)
	node[val].val = triple[tmp]
	node[val].code = helper_concatcode(node[val].code, node[val2].code, {tmp})
	
	return val
end

function process_addiexpr_minus_multiexpr()
	local val = parser_getstackcontent(-3)
	local val2 = parser_getstackcontent(-1)
	local tmp = helper_gencode('-', node[val].val, node[val2].val)
	node[val].val = triple[tmp]
	node[val].code = helper_concatcode(node[val].code, node[val2].code, {tmp})
	
	return val
end

function process_postfixexpr_pplus()
	local val = parser_getstackcontent(-2)
	local tmp = helper_gencode('+', node[val].val, 1)
	local tmp2 = helper_gencode('=', node[val].val, triple[tmp])
	
	node[val].code = helper_concatcode(node[val].code, {tmp, tmp2})
	node[val].val = tmp2
	
	return val
end

function process_postfixexpr_mminus()
	local val = parser_getstackcontent(-2)
	local tmp = helper_gencode('-', node[val].val, 1)
	local tmp2 = helper_gencode('=', node[val].val, triple[tmp])
	
	node[val].code = helper_concatcode(node[val].code, {tmp, tmp2})
	node[val].val = tmp2
	
	return val
end

function process_postfixexpr_lbk_expr_rbk()
	local val = parser_getstackcontent(-4)
	local val2 = parser_getstackcontent(-2)
	
	if(helper_checkdefined(val) == false) then
		return -1
	end
	
	if(node[val].addr.type ~= "array") then
		print "Grammar Error"
		return -1
	end
	
	--[[if(type(node[val2].val) ~= "number") then
		print "Grammar Error"
		return -1
	end--]]
	
	local tmp = helper_gencode('AN', node[val].addr, node[val2].val)
	--local tmp2 = helper_gencode('=', node[tmp], )
	node[val].code = helper_concatcode(node[val].code, node[val2].code, {tmp})
	node[val].val = triple[tmp]
	
	return val
end

function process_return_expr()
	local val = parser_getstackcontent(-2)
	
	node[val].code = helper_concatcode(node[val].code, {helper_gencode('R', node[val].val)})
	
	return val
end

function process_relexpr_less_addiexpr()
	local val1 = parser_getstackcontent(-3)
	local val2 = parser_getstackcontent(-1)
	
	local tmp = helper_gencode('<', node[val1].val, node[val2].val)
	local tmp1 = helper_gencode('IF', triple[tmp], nil)
	--local tmp2 = helper_gencode('G', nil, nil)
	
	node[val1].code = helper_concatcode(node[val1].code, node[val2].code, {tmp, tmp1})
	node[val1].falselist = {triple[tmp1]}
	--node[val1].truelist = {triple[tmp2]}
	
	return val1
end

function process_relexpr_greater_addiexpr()
	local val1 = parser_getstackcontent(-3)
	local val2 = parser_getstackcontent(-1)
	
	local tmp = helper_gencode('>', node[val1].val, node[val2].val)
	local tmp1 = helper_gencode('IF', triple[tmp], nil)
	--local tmp2 = helper_gencode('G', nil, nil)
	
	node[val1].code = helper_concatcode(node[val1].code, node[val2].code, {tmp, tmp1})
	node[val1].falselist = {triple[tmp1]}
	--node[val1].truelist = {triple[tmp2]}
	
	return val1
end

function process_relexpr_lequ_addiexpr()
	local val1 = parser_getstackcontent(-3)
	local val2 = parser_getstackcontent(-1)
	
	local tmp = helper_gencode('<=', node[val1].val, node[val2].val)
	local tmp1 = helper_gencode('IF', triple[tmp], nil)
	--local tmp2 = helper_gencode('G', nil, nil)
	
	node[val1].code = helper_concatcode(node[val1].code, node[val2].code, {tmp, tmp1})
	node[val1].falselist = {triple[tmp1]}
	--node[val1].truelist = {triple[tmp2]}
	
	return val1
end

function process_relexpr_gequ_addiexpr()
	local val1 = parser_getstackcontent(-3)
	local val2 = parser_getstackcontent(-1)
	
	local tmp = helper_gencode('>=', node[val1].val, node[val2].val)
	local tmp1 = helper_gencode('IF', triple[tmp], nil)
	--local tmp2 = helper_gencode('G', nil, nil)
	
	node[val1].code = helper_concatcode(node[val1].code, node[val2].code, {tmp, tmp1})
	node[val1].falselist = {triple[tmp1]}
	--node[val1].truelist = {triple[tmp2]}
	
	return val1
end

function process_equexpr_notequ_relexpr()
	local val1 = parser_getstackcontent(-3)
	local val2 = parser_getstackcontent(-1)
	
	local tmp = helper_gencode('!=', node[val1].val, node[val2].val)
	local tmp1 = helper_gencode('IF', triple[tmp], nil)
	--local tmp2 = helper_gencode('G', nil, nil)
	
	node[val1].code = helper_concatcode(node[val1].code, node[val2].code, {tmp, tmp1})
	node[val1].falselist = {triple[tmp1]}
	node[val1].truelist = {triple[tmp2]}
	
	return val1
end

function process_blockitemlist()
	local listval = parser_getstackcontent(-2)
	local val = parser_getstackcontent(-1)
	
	node[listval].code = helper_concatcode(node[listval].code, node[val].code)
	
	return listval
end

function process_ifstmt()
	local expval = parser_getstackcontent(-3)
	local stmtval = parser_getstackcontent(-1)
	local label_ifend_tri
	local label_ifend_id
	label_ifend_tri, label_ifend_id = helper_genlabel()
	
	helper_patchback(node[expval].falselist, label_ifend_id)
	
	node[expval].code = helper_concatcode(node[expval].code, node[stmtval].code, {label_ifend_tri})
	node[expval].val = nil;
	
	return expval
end

function process_ifelsestmt()
	local expval = parser_getstackcontent(-5)
	local stmt1val = parser_getstackcontent(-3)
	local stmt2val = parser_getstackcontent(-1)
	local label_elsebegin_tri
	local label_elsebegin_id
	local label_elseend_tri
	local label_elseend_id
	label_elsebegin_tri, label_elsebegin_id = helper_genlabel()
	label_elseend_tri, label_elseend_id = helper_genlabel()
	
	
	helper_patchback(node[expval].falselist, label_elsebegin_id)
	
	node[expval].code = helper_concatcode(node[expval].code, 
											node[stmt1val].code, 
											{helper_gencode('G', nil, label_elseend_id)}, 
											{label_elsebegin_tri},
											node[stmt2val].code,
											{label_elseend_tri})

	return expval
end

function process_whilestmt()
	local expval = parser_getstackcontent(-3)
	local stmtval = parser_getstackcontent(-1)
	
	local label_whilebegin_tri
	local label_whilebegin_id
	local label_whileend_tri
	local label_whileend_id
	label_whilebegin_tri, label_whilebegin_id = helper_genlabel()
	label_whileend_tri, label_whileend_id = helper_genlabel()
	
	helper_patchback(node[expval].falselist, label_whileend_id)
	
	node[expval].code = helper_concatcode({label_whilebegin_tri},
											node[expval].code,
											node[stmtval].code, 
											{helper_gencode('G', nil, label_whilebegin_id)},
											{label_whileend_tri})
	
	return expval
end

function process_forstmt()
	local exp1 = parser_getstackcontent(-7)
	local exp2 = parser_getstackcontent(-5)
	local exp3 = parser_getstackcontent(-3)
	local stmt = parser_getstackcontent(-1)
	local label_forbegin_tri
	local label_forbegin_id
	local label_forend_tri
	local label_forend_id
	label_forbegin_tri, label_forbegin_id = helper_genlabel()
	label_forend_tri, label_forend_id = helper_genlabel()
	
	helper_patchback(node[exp2].falselist, label_forend_id)
	
	node[exp1].code = helper_concatcode(node[exp1].code, {label_forbegin_tri}, node[exp2].code, node[stmt].code, 
										node[exp3].code, {helper_gencode('G', nil, label_forbegin_id), label_forend_tri})
	
	return exp1
end

function process_postfixexpr_lp_arglist_lr()
	local expval = parser_getstackcontent(-4)
	local argval = parser_getstackcontent(-2)
	local tmp = 0
	
	node[expval].code = helper_concatcode(node[expval].code, node[argval].code)
	
	--for i = #node[argval].arglist, 1, -1 do
		tmp = helper_gencode('param', node[argval].arglist)
		table.insert(node[expval].code, tmp)
	--end
	
	table.insert(node[expval].code, helper_gencode('call', node[expval].val))
	table.insert(node[expval].code, helper_gencode('param_num', #node[argval].arglist))
	
	return expval
end

function process_funcdef()
	local rtype = parser_getstackcontent(-3)
	local decl = parser_getstackcontent(-2)
	local block = parser_getstackcontent(-1)
	
	node[block].funcname = node[decl].addr
	node[block].type = node[rtype].type
	
--	print_lua_table(node[block])
	
	return block
end

------------------------------------------------------

--[=[function produce_block_entrance()
	local lable_num = 0
	for i, v in pairs(final_code) do
		if triple[v][1] == 'G' or triple[v][1]=='IF' then
			if block_entrance[i+1] == nil then
				block_entrance[i+1] = lable_num
				lable_num = lable_num + 1
			end
			if block_entrance[i+1+triple[v][3]] == nil then
				block_entrance[i+1+triple[v][3]] = lable_num
				lable_num = lable_num + 1
			end
		end
	end
end
--]=]

function get_reg()
	
end

use_info = {}

function update_use_info(item, active, useid)
	if item ~= nil then
		use_info[item] = {}
		use_info[item].state = active
		use_info[item].useid = useid
	end
end

function get_use_state(item)
	local ret = nil
	if type(item) ~= "number" and type(item) ~= "string" and use_info[item] ~= nil then
		ret = use_info[item].state
	end
	return ret
end

function get_use_id(item)
	local ret = nil
	if use_info[item] ~= nil then
		ret = use_info[item].useid
	end
	return ret
end

function produce_use_info()
	local t1 = nil
	local t2 = nil
	local t3 = nil
	
	for i = #final_code, 1, -1 do
		if triple[final_code[i]][1] == 'L'then
			use_info = {}
			final_code[i] = {final_code[i], {}}
		else
			if triple[final_code[i]][1] == 'IF' or triple[final_code[i]][1] == 'G' then
				use_info = {}
			end
		
			if triple[final_code[i]][1] == '=' then
				t1 = triple[final_code[i]][2]
				t2 = triple[final_code[i]][3]
				t3 = nil
			else
				t1 = triple[final_code[i]]
				t2 = triple[final_code[i]][2]
				t3 = triple[final_code[i]][3]
			end

			if type(t2) == "number" or type(t2) == "string" then
				t2 = nil
			end
			
			if type(t3) == "number" or type(t3) == "string" then
				t3 = nil
			end

			final_code[i] = {final_code[i], {get_use_state(t1), get_use_id(t1), get_use_state(t2), get_use_id(t2), get_use_state(t3), get_use_id(t3)}}

			local x = get_use_state(t2)
			local y = get_use_id(t2)
			
			if x == nil then
				x = ""
			end
			
			if y == nil then
				y = ""
			end

			print(i .. " " .. tostring(x) .. " " .. y)
			update_use_info(t1, false, nil)
			update_use_info(t2, true, i)
			update_use_info(t3, true, i)
		end
	end
end

reg_table = {
	eax = {},
	ebx = {},
	ecx = {},
	edx = {},
	esi = {},
	edi = {}
}
addr_table = {}

asm_code = {}

function gen_asmcode(code)
	table.insert(asm_code, code)
end

function reg_name(reg)
	local name = ""
	if reg == reg_table.eax then
		name = "%eax"
	elseif reg == reg_table.ebx then
		name = "%ebx"
	elseif reg == reg_table.ecx then
		name = "%ecx"
	elseif reg == reg_table.edx then
		name = "%edx"
	elseif reg == reg_table.esi then
		name = "%esi"
	elseif reg == reg_table.edi then
		name = "%edi"
	end
	return name
end

function get_used_reg(whom)
		local reg = nil
		for i, v in pairs(reg_table) do
			if v[1] == whom then
				reg = v
				break
			end
		end
		return reg
end

function clear_used_reg(whom)
		for i, v in pairs(reg_table) do
			if v[1] == whom then
				reg_table[i] = {}
				break
			end
		end
end

function get_reg(forwhom)
	local reg = nil
	--[[if y ~= nil and addr_table[y] ~= nil and addr_table[y].reg ~= nil and #addr_table[y].reg == 1 
		and y.addr == nil and final_code[cur_code][2][4] == nil then
			reg = addr_table[y].reg
			addr_table[y].reg = nil
	else--]]
		for i, v in pairs(reg_table) do
			if #v == 0 then
				reg = v
			end
		end
		if reg == nil then
			helper_print_asm()
			error "No useable reg."
		end
		table.insert(reg, forwhom)
		return reg
	--end
end

function update_reg(cur_code)
	local y = triple[final_code[cur_code][1]][2]
	local z = triple[final_code[cur_code][1]][3]
	
	--print(final_code[cur_code][2][4])
	
	if y ~= nil and final_code[cur_code][2][4] == nil then
			clear_used_reg(y)
			--print(get_used_reg(y))
	end
	
	if z ~= nil and final_code[cur_code][2][6] == nil then
			clear_used_reg(z)
	end
end

function helper_get_asm()
	local code = ""
	for i, v in pairs(asm_code) do
		code = code .. v .. "\n"
	end
	
	return code
end

function helper_print_asm()
	print(helper_get_asm())
end

function gen_tar(x, need_reg, indirect)
	if type(x) == "number" then
		local r = reg_name(get_reg(x))
		gen_asmcode("mov $" .. x .. ", " .. r)
		clear_used_reg(x)
		return r
	elseif type(x) == "string" then
		return x
	elseif x.type ~= "array" and x.name ~= nil and not need_reg then
		return x.name
	elseif get_used_reg(x) ~= nil then
		if indirect then
			return "(" .. reg_name(get_used_reg(x)) .. ")"
		else
			return reg_name(get_used_reg(x))
		end
	else
		--[[local reg = get_reg(x)
		gem_asmcode("mov " .. x.name .. ", " .. reg_name(reg))
		return reg_name(reg)--]]
		--helper_print_asm()
		--print_lua_table(x)
		error "asdf"
	end
end

function gen_src(x)

end

function ensure_in_reg(x)
	if get_used_reg(x) == nil then
		gen_asmcode("mov " .. x.name .. ", " .. reg_name(get_reg(x)))
	end
end

function generate_asm_code()
	local cur_triple = nil
	for i = 1, #final_code do
		cur_triple = triple[final_code[i][1]]
		print(i)
		if cur_triple[1] == 'L' then
			gen_asmcode("L" .. cur_triple[2] .. ":")
		elseif cur_triple[1] == 'A' then
			--print_lua_table(triple[final_code[i][1]][2])
			if(get_used_reg(cur_triple[2]) == nil) then
				gen_asmcode("lea " .. cur_triple[2].name .. ", " ..reg_name(get_reg(cur_triple[2])))
			end
			
			local base = ""
			local offset = ""
			if type(cur_triple[3]) == "number" then
				base = 4 * cur_triple[3]
			else--[[if cur_triple[3].name ~= nil then
				--gen_asmcode("mov " .. gen_tar(cur_triple[3]) .. ", " .. reg_name(get_reg(cur_triple[3])))
				--base = reg_name(get_used_reg(cur_triple[3]))
				--clear_used_reg(cur_triple[3])
				base = cur_triple[3].name
			else--]]
				gen_asmcode("mov " .. gen_tar(cur_triple[3]) .. ", " .. reg_name(get_reg(cur_triple[3])))
				offset = reg_name(get_used_reg(cur_triple[3]))
				clear_used_reg(cur_triple[3])
				offset = ", " .. offset .. ", 4"
			end
			gen_asmcode("lea " .. base .. "(" .. gen_tar(cur_triple[2]) .. offset .. "), " .. reg_name(get_reg(cur_triple)))
		elseif cur_triple[1] == 'AN' then
			if(get_used_reg(cur_triple[2]) == nil) then
				gen_asmcode("lea " .. cur_triple[2].name .. ", " ..reg_name(get_reg(cur_triple[2])))
			end
			
			local base = ""
			local offset = ""
			if type(cur_triple[3]) == "number" then
				base = 4 * cur_triple[3]
			else--[[if cur_triple[3].name ~= nil then
				--gen_asmcode("mov " .. gen_tar(cur_triple[3]) .. ", " .. reg_name(get_reg(cur_triple[3])))
				--base = reg_name(get_used_reg(cur_triple[3]))
				--clear_used_reg(cur_triple[3])
				base = cur_triple[3].name
			else--]]
				gen_asmcode("mov " .. gen_tar(cur_triple[3]) .. ", " .. reg_name(get_reg(cur_triple[3])))
				offset = reg_name(get_used_reg(cur_triple[3]))
				clear_used_reg(cur_triple[3])
				offset = ", " .. offset .. ", 4"
			end
			gen_asmcode("mov " .. base .. "(" .. gen_tar(cur_triple[2]) .. offset .. "), " .. reg_name(get_reg(cur_triple)))
		elseif cur_triple[1] == '=' then
			--print_lua_table(triple[final_code[i][1]])
			--if triple[final_code[i][1]].name ~= nil then  --symbol
			gen_asmcode("mov " .. gen_tar(cur_triple[3]) .. ", " .. gen_tar(cur_triple[2], false, true))
			--else	-- temp
				--gen_asmcode("mov " .. reg_name(get_used_reg(triple[final_code[i][1]][3])) .. ", " .. reg_name(get_used_reg(triple[final_code[i][1]][2].name)))
			--end
		elseif cur_triple[1] == '<'
			or cur_triple[1] == '!=' 
			or cur_triple[1] == '>=' then
			gen_asmcode("cmp " .. gen_tar(cur_triple[3]) .. ", " .. gen_tar(cur_triple[2]))

		elseif cur_triple[1] == 'IF' then
			local cmp_sym = triple[final_code[i-1][1]][1];
			if cmp_sym == '<' then
				gen_asmcode("jnl L" .. cur_triple[3])
			elseif cmp_sym == '!=' then
				gen_asmcode("je L" .. cur_triple[3])
			elseif cmp_sym == '>=' then
				gen_asmcode("jl L" .. cur_triple[3])
			end
		elseif cur_triple[1] == '*' then
			gen_asmcode("push %rax")
			gen_asmcode("push %rdx")
			local eax = reg_table.eax
			local edx = reg_table.edx
			gen_asmcode("mov " .. gen_tar(cur_triple[2]) .. ", " .. " %eax")
			reg_table.eax = cur_triple
			reg_table.edx = cur_triple
			ensure_in_reg(cur_triple[3])
			gen_asmcode("imul " .. reg_name(get_used_reg(cur_triple[3])))
			gen_asmcode("mov %eax, " .. reg_name(get_reg(cur_triple)))
			reg_table.eax = eax
			reg_table.edx = edx
			gen_asmcode("pop %rdx")
			gen_asmcode("pop %rax")
		elseif cur_triple[1] == '/' then
			gen_asmcode("push %rax")
			gen_asmcode("push %rdx")
			local eax = reg_table.eax
			local edx = reg_table.edx
			gen_asmcode("mov " .. gen_tar(cur_triple[2]) .. ", " .. " %eax")
			reg_table.eax = cur_triple
			reg_table.edx = cur_triple
			gen_asmcode("cdq")
			ensure_in_reg(cur_triple[3])
			gen_asmcode("idiv " .. reg_name(get_used_reg(cur_triple[3])))
			gen_asmcode("mov %eax, " .. reg_name(get_reg(cur_triple)))
			reg_table.eax = eax
			reg_table.edx = edx
			gen_asmcode("pop %rdx")
			gen_asmcode("pop %rax")
		elseif cur_triple[1] == '+' then
			gen_asmcode("mov " .. gen_tar(cur_triple[2]) .. ", " .. reg_name(get_reg(cur_triple)))
			gen_asmcode("add " .. gen_tar(cur_triple[3]) .. ", " .. reg_name(get_used_reg(cur_triple)))
		elseif cur_triple[1] == '-' then
			gen_asmcode("mov " .. gen_tar(cur_triple[2]) .. ", " .. reg_name(get_reg(cur_triple)))
			gen_asmcode("sub " .. gen_tar(cur_triple[3]) .. ", " .. reg_name(get_used_reg(cur_triple)))
		elseif cur_triple[1] == 'G' then
			gen_asmcode("jmp L" .. cur_triple[3])
		elseif cur_triple[1] == 'param' then
			gen_asmcode("push %rdi")
			gen_asmcode("push %rsi")
			for i = #cur_triple[2], 3, -1 do
				local p
				if type(cur_triple[2]) == "string" then
					p = cur_triple[2]
				else
					p = cur_triple[2].name
				end
				gen_asmcode("push " .. p)
			end
			print_lua_table(cur_triple[2])
			if #cur_triple[2] >= 2 then
				gen_asmcode("mov " .. gen_tar(cur_triple[2][2]) .. ", %rsi")
			end
			print_lua_table(cur_triple[2])
			gen_asmcode("mov " .. gen_tar(cur_triple[2][1]) .. ", %rdi")
			
		elseif cur_triple[1] == 'call' then
			gen_asmcode("call " .. cur_triple[2].name)
		elseif cur_triple[1] == 'param_num' then
			local n = 8 * (tonumber(cur_triple[2]) - 2)
			if n ~= 0 then
				gen_asmcode("add $" .. n .. ", %rsp")
			end
			gen_asmcode("pop %rsi")
			gen_asmcode("pop %rdi")
		elseif cur_triple[1] == 'R' then
			gen_asmcode("mov $" .. cur_triple[2] .. ", %rdi")
			gen_asmcode("call exit")
		else
			helper_print_asm()
			error("Not support instruction: " .. cur_triple[1])
		end
		
		update_reg(i)
	end
end

function calc_size(v)
	if v.type == "int" then
		return 4
	elseif v.type == "array" then
		return v.arrnum * 4
	else
		return 0
	end
end

function helper_get_asmheader()
	local header = ".section .data\n"
	
	for i, v in pairs(strings) do
		header = header .. "__STRING_" .. i .. ": .asciz " .. v .. "\n"
	end
	
	header = header .. ".section .bss\n"
	
	for i, v in pairs(symtable) do
		local size = calc_size(v)
		if size ~= 0 then
			header = header .. ".lcomm " .. v.name .. ", " .. size .. "\n"
		end
	end
	
	header = header .. ".section .text\n\
.globl _start\n\
_start:\n"

	return header
end

function helper_print_asmheader()
	print(helper_get_asmheader())
end

function helper_write_to_file(file_name)
	local f = assert(io.open(file_name, 'w'))
	f:write(helper_get_asmheader())
	f:write(helper_get_asm())
	f:close()
end

function parse_complete()
	--helper_dump_symtable()
	--helper_dump_node()
	--helper_dump_triple()
	final_code = node[parser_getstackcontent(-1)].code
	--print_lua_table(final_code)
	helper_print_code()
	produce_use_info()
	generate_asm_code()
	--helper_print_asmheader()
	--helper_print_asm()
	helper_write_to_file("/home/dontpanic/hello.S")
	--helper_print_use_info()
	--print_lua_table(block_entrance)
	--print_lua_table(final_code)
end
