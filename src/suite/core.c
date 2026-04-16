/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include <string.h>

#include "core.h"

const char *MODE_CHAT = "chat";
const char *MODE_CODE = "code";
const char *MODE_PAPER = "paper";
const char *MODE_CORPUS = "corpus";
const char *MODE_AGENT = "agent";

const char *get_system_prompt(const char *mode)
{
    if (!strcmp(mode, MODE_CODE))
        return "You are a coding agent. Plan briefly, execute with small diffs, verify. "
               "Prefer editing existing files over wholesale rewrites.";
    if (!strcmp(mode, MODE_PAPER))
        return "You are drafting a technical paper in LaTeX. Use clear structure "
               "(abstract, definitions, results, discussion, references). "
               "Keep claims aligned with evidence you can cite.";
    if (!strcmp(mode, MODE_CORPUS))
        return "You answer using local corpus files when available; say when evidence is missing.";
    if (!strcmp(mode, MODE_AGENT))
        return "You are an autonomous agent: decompose the task, use tools, verify, report. "
               "Ask before destructive actions.";
    return "You are a helpful assistant.";
}

const char **get_tools_for_mode(const char *mode, int *n_tools)
{
    static const char *chat_tools[] = {"search_web", "search_corpus"};
    static const char *code_tools[] = {"read_file", "write_file", "edit_file", "run_command", "grep",
                                       "list_dir", "git_diff", "git_commit", "git_status"};
    static const char *paper_tools[] = {"read_file", "write_file", "latex_compile", "search_corpus",
                                        "search_web"};
    static const char *corpus_tools[] = {"read_file", "search_corpus", "memory_recall", "memory_store"};
    static const char *agent_tools[] = {"read_file", "write_file", "edit_file", "run_command", "grep",
                                        "list_dir", "git_diff", "git_commit", "search_web", "latex_compile",
                                        "memory_recall", "memory_store"};

    if (!strcmp(mode, MODE_CODE)) {
        *n_tools = (int)(sizeof code_tools / sizeof code_tools[0]);
        return code_tools;
    }
    if (!strcmp(mode, MODE_PAPER)) {
        *n_tools = (int)(sizeof paper_tools / sizeof paper_tools[0]);
        return paper_tools;
    }
    if (!strcmp(mode, MODE_CORPUS)) {
        *n_tools = (int)(sizeof corpus_tools / sizeof corpus_tools[0]);
        return corpus_tools;
    }
    if (!strcmp(mode, MODE_AGENT)) {
        *n_tools = (int)(sizeof agent_tools / sizeof agent_tools[0]);
        return agent_tools;
    }
    *n_tools = (int)(sizeof chat_tools / sizeof chat_tools[0]);
    return chat_tools;
}
