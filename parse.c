#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h>

static struct compile_process* current_process;
static struct token* parser_last_token;

extern struct expressionable_op_precedence_group op_precedence[TOTAL_OPERATOR_GROUPS];

// history 的含义未知
struct history {
    int flags;
};

// 这两个history函数是在干嘛？
struct history* history_begin(int flags) {
    struct history* history = calloc(1, sizeof(struct history));
    history->flags = flags;
    return history;
}

struct history* history_down(struct history* history, int flags) {
    struct history* new_history = calloc(1, sizeof(struct history));
    memcpy(history, new_history, sizeof(struct history));
    new_history->flags = flags;
    return new_history;
}

static int parse_expressionable_single(struct history* history);
static void parse_expressionable(struct history* history);

static void parser_ignore_nl_or_comment(struct token* token) {
    while(token && token_is_nl_or_comment_or_newline_seperator(token)) {
        vector_peek(current_process->token_vec);
        token = vector_peek_no_increment(current_process->token_vec);
    }
}

static struct token* token_next() {
    struct token* next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    current_process->pos = next_token->pos;
    parser_last_token = next_token;
    return vector_peek(current_process->token_vec);
}

void parse_single_token_to_node() {
    struct token* token = token_next();
    struct node* node = NULL;
    switch(token->type) {
        // 数字，变量和字符串可以直接制作成 node
        case TOKEN_TYPE_NUMBER:
            node = node_create(&(struct node){.type = NODE_TYPE_NUMBER, .llnum = token->llnum});
        break;

        case TOKEN_TYPE_IDENTIFIER:
            node = node_create(&(struct node){.type = NODE_TYPE_IDENTIFIER, .sval = token->sval});
        break;

        case TOKEN_TYPE_STRING:
            node = node_create(&(struct node){.type = NODE_TYPE_STRING, .sval = token->sval});
        break;

        default:
            compile_error(current_process, "This %d not a single token that can be converted to a node", token->type);
    }
}

static struct token* token_peek_next() {
    struct token* token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(token);
    return vector_peek_no_increment(current_process->token_vec);
}

// 把token序列看作一个中序遍历的一棵树
static void parse_expressionable_for_op(struct history* history, const char* op) {
    parse_expressionable(history);
}

static int parser_get_precedence_for_operator(const char* op, struct expressionable_op_precedence_group** group_out) {
    *group_out = NULL;
    for(int i = 0; i < TOTAL_OPERATOR_GROUPS; ++i) {
        for(int b = 0; op_precedence[i].operators[b]; ++b) {
            const char* _op = op_precedence[i].operators[b];
            if(S_EQ(op, _op)) {
                *group_out = &op_precedence[i];
                return i;
            }
        }
    }

    return -1;
}

static bool parser_left_op_has_priority(const char* op_left, const char* op_right) {
    struct expressionable_op_precedence_group* group_left;
    struct expressionable_op_precedence_group* group_right;

    if(S_EQ(op_left, op_right)) {
        return false;
    }

    int precdence_left = parser_get_precedence_for_operator(op_left, &group_left);
    int precdence_right = parser_get_precedence_for_operator(op_right, &group_right);
    if(group_left->associativity == ASSOCIATIVITY_RIGHT_TO_LEFT) {
        return false;
    }

    return precdence_left <= precdence_right;
}

static void parser_node_shift_children_left(struct node* node) {
    assert(node->type == NODE_TYPE_EXPRESSION);
    assert(node->type == NODE_TYPE_EXPRESSION);

    // E(50 * E(30 + 20)) * 的优先级大于 +
    const char* right_op = node->exp.right->exp.op;
    struct node* new_exp_left_node = node->exp.left;
    struct node* new_exp_right_node = node->exp.right->exp.left;
    make_exp_node(new_exp_left_node, new_exp_right_node, node->exp.op);

    // 先创建一个 E(50 * 30)
    struct node* node_left_operand = node_pop();
    
    // 再创建一个 20
    struct node* node_right_operand = node->exp.right->exp.right;

    // 最后创建出 E(E(50 * 30) + 20)，其实就是做了一次左旋
    // 目前没有考虑带括号的情况，操作的只是指针和op，node的数量没有减少，反而增加了一个
    node->exp.left = node_left_operand;
    node->exp.right = node_right_operand;
    node->exp.op = right_op;
}

static void parser_reorder_expression(struct node** node_out) {
    struct node* node = *node_out;
    if(node->type != NODE_TYPE_EXPRESSION) {
        return;
    }

    if(node->exp.left->type != NODE_TYPE_EXPRESSION &&
        node->exp.right && node->exp.right->type != NODE_TYPE_EXPRESSION) {
            return;
    }

    if(node->exp.left->type != NODE_TYPE_EXPRESSION &&
        node->exp.right && node->exp.right->type == NODE_TYPE_EXPRESSION) {
        const char* right_op = node->exp.right->exp.op;
        if(parser_left_op_has_priority(node->exp.op, right_op)) {
            parser_node_shift_children_left(node);

            parser_reorder_expression(&node->exp.left);
            parser_reorder_expression(&node->exp.right);
        }
    }
}

static void prase_exp_normal(struct history* history) {
    struct token* op_token = token_peek_next();
    const char* op = op_token->sval;
    struct node* node_left = node_peek_expressionable_or_null();
    if(!node_left) {
        return;
    }

    token_next();

    node_pop();
    node_left->flags |= NODE_FLAG_INSIDE_EXPRESSION;
    parse_expressionable_for_op(history_down(history, history->flags), op);
    struct node* node_right = node_pop();
    node_right->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    // 可以预见这样可以构建一个表达式的语法树
    make_exp_node(node_left, node_right, op);
    struct node* exp_node = node_pop();

    // node_print(stdout, exp_node);
    // node_print(stdout, exp_node->exp.left);
    // node_print(stdout, exp_node->exp.right);

    parser_reorder_expression(&exp_node);

    // 从这里可以看出来，node_vec 就是一个临时node数组，而 node_tree_vec 存的是所有语法树的根节点
    node_push(exp_node);
}

static int parse_exp(struct history* history) {
    prase_exp_normal(history);
    return 0;
}

static int parse_expressionable_single(struct history* history) {
    struct token* token = token_peek_next();
    if(!token) {
        return -1;
    }

    history->flags |= NODE_FLAG_INSIDE_EXPRESSION;
    int res = -1;
    switch(token->type) {
        case TOKEN_TYPE_NUMBER:
            parse_single_token_to_node();
            res = 0;
        break;

        case TOKEN_TYPE_OPERATOR:
            parse_exp(history);
            res = 0;
        break;
    }
    return res;
}

static void parse_expressionable(struct history* history) {
    while(parse_expressionable_single(history) == 0) {
        
    }
}

int parse_next() {
    struct token* token = token_peek_next();

    if(!token) {
        return -1;
    }

    int res = 0;
    switch(token->type) {
        case TOKEN_TYPE_NUMBER:
        case TOKEN_TYPE_IDENTIFIER:
        case TOKEN_TYPE_STRING:
        case TOKEN_TYPE_OPERATOR:
            parse_expressionable(history_begin(0));

        break;
    }

    return 0;
}

static void dfs_print_node(FILE* fp, struct node* node) {
    if(node == NULL) return;
    
    node_print(fp, node);

    if(node->type == NODE_TYPE_EXPRESSION) dfs_print_node(fp, node->exp.left);

    if(node->type == NODE_TYPE_EXPRESSION) dfs_print_node(fp, node->exp.right);
}

int parse(struct compile_process* process) {
    current_process = process;
    
    parser_last_token = NULL;
    node_set_vector(process->node_vec, process->node_tree_vec);
    struct node* node = NULL;
    vector_set_peek_pointer(process->token_vec, 0);
    while(parse_next() == 0) {
        node = node_peek();
        vector_push(process->node_tree_vec, &node); 
    }

    FILE* fp = fopen("./file/node.txt", "w");
    fprintf(fp, "一共有 %d 颗语法树\n", process->node_tree_vec->count);
    for(int i = 0; i < process->node_tree_vec->count; ++i) {
        fprintf(fp, "第 %d 颗语法树的前序序列\n", i + 1);
        dfs_print_node(fp, *(struct node**)vector_at(process->node_tree_vec, i));    
        fprintf(fp, "\n");
    }
    fclose(fp);

    return PARSE_ALL_OK;
}