#include <iostream>
#include <string>
#include <vector>

#include "code_gen.h"

int Label::counter = 0;
int Global::counter = 0;
int StrGlobal::counter = 0;

std::string current_func;
int current_offset;

std::map<void*, std::string> vars = {};
std::map<std::string, std::string> strings = {
		{StrGlobal().to_string(), ""}
};

std::vector<std::string> all_registers {
		"$t9",
		"$t8",
		"$t7",
		"$t6",
		"$t5",
		"$t4",
		"$t3",
		"$t2",
		"$t1",
		"$t0",
};
std::map<std::string, std::vector<std::string>> available_registers {};

void populate_registers(std::string func){
	for (auto r : all_registers) {
		available_registers[func].push_back(r);
	}
}

std::string alloc_reg(){
	if (available_registers.find(current_func) == available_registers.end()) {
		populate_registers(current_func);
	}

	if (!available_registers[current_func].empty()) {
		std::string available_reg = available_registers[current_func].back();
		available_registers[current_func].pop_back();
		return available_reg;
	}

	else {
		std::cout << "error: not enough free registers" << std::endl;
		exit(1);
	}
}

void freereg(std::string reg){
	if (reg != "" && reg.substr(0, 2) != "$v" && reg.substr(0, 2) != "$a") {
		// cout << "    # Deallocated reg: " << reg << endl;
		available_registers[current_func].push_back(reg);
	}
}

void emit(std::string line) {
	std::cout << line << std::endl;
}

void gen_pass_0(AST *ast) {
	if (ast->type == "program") {
		for (auto child: ast->children) {
			gen_pass_0(child);
		}
	} else if (ast->type == "globalvar") {
		auto global = Global();
		emit("    .data");
		emit(global.to_string() + ":");
		if (ast->get_child(1)->attr == "string") {
			emit("    .word S0");
		} else {
			emit("    .word 0");
		}
		emit("    .text");
		vars[ast->sym] = global.to_string();
	}
}

void gen_pass_1(AST *ast) {
	if (ast->type == "program") {
		for (auto child: ast->children) {
			gen_pass_1(child);
		}
	}

	else if (ast->type == "func") {
		// Setup stack frame
		emit(ast->get_child(0)->attr + ":");
		auto locals = count_locals(ast->get_child(2));
		auto formals = ast->get_child(1)->get_child(0)->children.size();
		auto frame_size = (locals + formals) * 4 + 4;
		emit("    subu $sp,$sp," + std::to_string(frame_size));
		emit("    sw $ra,0($sp)");

		// Store parameters
		int i = 0;
		for(auto formal : ast->get_child(1)->get_child(0)->children) {
			auto offset = i * 4 + 4;
			emit("    sw $a" + std::to_string(i) + "," + std::to_string(offset) + "($sp)");
			vars[formal->get_child(0)->sym] = std::to_string(offset) + "($sp)";
			i++;
		}

		// Body
		current_offset = 4;
		gen_pass_1(ast->get_child(2));

		// Epilogue
		emit(ast->get_child(0)->attr + "_epilogue:");
		emit("    lw $ra,0($sp)");
		emit("    addu $sp,$sp," + std::to_string(frame_size));
		emit("    jr $ra");
	}

	else if (ast->type == "funccall") {
		/**
		 * funccall sig=void @ (2, 8)
				id [foo] sig=f(int,int,int,int) sym=0x565087163f00 @ (2, 5)
				actuals
					int [10] sig=int @ (2, 9)
					int [20] sig=int @ (2, 13)
					int [30] sig=int @ (2, 17)
					int [40] sig=int @ (2, 21)
		 */
		// Pass parameters
		int i = 0;
		for(auto actual : ast->get_child(1)->children) {
			gen_pass_1(actual);
			i++;
		}
		i = 0;
		for(auto actual : ast->get_child(1)->children) {
			emit("    move $a" + std::to_string(i) + "," + actual->reg);
			freereg(actual->reg);
			i++;
		}
		emit("    jal " + ast->get_child(0)->attr);
	}

	else if (ast->type == "var") {
		if(ast->get_child(1)->attr == "string") {
			emit("    la $v1,S0");
			emit("    sw $v1," + std::to_string(current_offset) + "($sp)");
		} else {
			emit("    sw $0," + std::to_string(current_offset) + "($sp)");
		}
		vars[ast->sym] = std::to_string(current_offset) + "($sp)";
		current_offset += 4;
	}

	else if (ast->type == "block") {
		for (auto child: ast->children) {
			gen_pass_1(child);
		}
	}

	else if (ast->type == "if") {
		emit("TODO IF");
	}

	else if (ast->type == "for") {
		emit("TODO FOR");
	}

	else if (ast->type == "break") {
		emit("TODO BREAK");
	}

	else if (ast->type == "return") {
		emit("TODO RETURN");
	}

	else if (ast->type == "int") {
		auto reg = alloc_reg();
		emit("    li " + reg + "," + ast->attr);
		ast->reg = reg;
	}

	else if (ast->type == "string") {
		auto str_global = StrGlobal();
		emit("    la $t0," + str_global.to_string());
		strings[str_global.to_string()] = ast->attr;
	}

	else if (ast->type == "id") {
		auto reg = alloc_reg();
		ast->reg = reg;
		if (ast->attr == "true") {
			emit("    li " + reg + ",Ltrue");
		} else if (ast->attr == "false") {
			emit("    li " + reg + ",Lfalse");
		} else {
			emit("    lw " + reg + "," + vars[ast->sym]);
		}
	}

	else if (ast->type == "&&") {
		emit("TODO AND");
	}

	else if (ast->type == "||") {
		emit("TODO OR");
	}

	else if (ast->type == "==") {
		emit("TODO EQUALS EQUALS");
	}

	else if (ast->type == "!=") {
		emit("TODO NOT EQUALS");
	}

	else if (ast->type == ">=") {
		emit("TODO GTOE");
	}

	else if (ast->type == ">") {
		emit("TODO GT");
	}

	else if (ast->type == "<=") {
		emit("TODO LTOE");
	}

	else if (ast->type == "<") {
		emit("TODO LT");
	}

	else if (ast->type == "*") {
		emit("TODO MUL");
	}

	else if (ast->type == "/") {
		emit("TODO DIV");
	}

	else if (ast->type == "%") {
		emit("TODO MOD");
	}

	else if (ast->type == "+") {
		gen_pass_1(ast->get_child(0));
		gen_pass_1(ast->get_child(1));
		auto reg = alloc_reg();
		ast->reg = reg;
		emit("    addu " + reg + "," + ast->get_child(0)->reg + "," + ast->get_child(1)->reg);
		freereg(ast->get_child(1)->reg);
		freereg(ast->get_child(0)->reg);
	}

	else if (ast->type == "-") {
		gen_pass_1(ast->get_child(0));
		gen_pass_1(ast->get_child(1));
		emit("    subu $t4,$t1,$t3");
	}

	else if (ast->type == "!") {
		emit("TODO NOT");
	}

	else if (ast->type == "=") {
		gen_pass_1(ast->get_child(1));
		emit("    sw " + ast->get_child(1)->reg + "," + vars[ast->get_child(0)->sym]);
		freereg		(ast->get_child(1)->reg);
	}
}

int count_locals(AST *ast) {
	int count = 0;
	if (ast->type == "var") {
		count++;
	} else if (ast->type == "block") {
		for (auto child: ast->children) {
			count += count_locals(child);
		}
	} else if (ast->type == "if") {
		count += count_locals(ast->get_child(1));
		if (ast->children.size() == 3) {
			count += count_locals(ast->get_child(2)->get_child(0));
		}
	} else if (ast->type == "for") {
		count += count_locals(ast->get_child(1));
	}
	return count;
}

void gen_pass_2() {
	if (strings.empty()) {
		return;
	}

	emit("    .data");
	for (auto &[label, value]: strings) {
		emit(label + ":");
		for (char &c: value) {
			emit("    .byte " + std::to_string(int(c)));
		}
		emit("    .byte 0");
	}
	emit("    .align 2");
	emit("    .text");
}

void generate_code(AST *root) {
	emit("    Ltrue = 1");
	emit("    Lfalse = 0");
	emit("    .text");
	emit("    .globl _start");
	emit("Lhalt:");
	emit("    li $v0,10");
	emit("    syscall");
	emit("    jr $ra");
	emit("_start:");
	emit("    jal main");
	emit("    j Lhalt");

	gen_pass_0(root);
	gen_pass_1(root);
	gen_pass_2();
//	root->pre([](auto ast) {
//		gen_pass1(ast, false);
//	});
}