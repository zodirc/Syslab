#pragma once

#include "../learn.hpp"

#include <cstddef>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace syslab {

struct Lesson {
  std::string group;
  /**
   * Reading track: 基础, 进阶, or 方向. Course lines mark the first and third
   * with the tokens base and direction. Unmarked lessons stay 进阶.
   * 阅读层次：基础、进阶或方向。课程行用 base 和 direction 标出第一层和第三层。未标记的课留在进阶。
   */
  std::string layer = "进阶";
  std::string id;
  std::string title;
  std::string diagnose;
  bool both = false;
  bool disasm = false;
};

struct RunView {
  std::string label;
  std::string run_directory;
  ExperimentState state = ExperimentState::Failed;
};

struct LessonBrief {
  /**
   * One-sentence definition of the term this lesson introduces. Empty when the
   * brief has no 名词 section.
   * 本课引入的名词，一句话。简报没有「名词」段时为空。
   */
  std::string term;
  std::string focus;
  std::string background;
  std::string language;
  std::string mechanism;
  std::string program;
  std::string principle;
};

struct LessonCopy {
  std::map<std::string, LessonBrief> briefs;
  std::map<std::string, std::string> groups;
};

std::string trim_copy(std::string text);
std::string markdown_section(const std::string& text, const char* heading);
std::string without_runner_notice(const std::string& text);
std::string display_path(const std::string& root, const std::string& path);
std::string html_escape(const std::string& text);
std::vector<std::string> split_fields(const std::string& line);
std::vector<Lesson> load_course(const std::string& root, const Registry& registry);
/**
 * @brief Selects the previous or next lesson that continues the same fact.
 * 选择与本节同一事实线的上一节或下一节。
 * @param lessons Course order.
 * 课程顺序。
 * @param index Index of the current lesson.
 * 当前节的下标。
 * @param direction -1 for the previous lesson, +1 for the next lesson.
 * -1 为上一节，+1 为下一节。
 * @param label Receives 上一事实, 下一事实, 进入下一层, or 另一方向.
 * 得到「上一事实」「下一事实」「进入下一层」或「另一方向」。
 * @return The selected lesson, or nullptr when that side has no lesson.
 * 选中的课；这一侧没有课时返回 nullptr。
 * @note Direction tracks stay inside one group. A different direction group is 另一方向.
 * 方向课只在同一组内衔接。另一组方向课标为「另一方向」。
 */
const Lesson* track_neighbor(const std::vector<Lesson>& lessons, std::size_t index, int direction, const char** label);
/**
 * @brief Finds a lesson by its experiment id.
 * 按实验编号查找一节课。
 * @param lessons Course order.
 * 课程顺序。
 * @param id Experiment id, such as CPU-001.
 * 实验编号，例如 CPU-001。
 * @return The lesson, or nullptr when the id is not in the course.
 * 找到的课；编号不在课程里时返回 nullptr。
 */
const Lesson* find_lesson(const std::vector<Lesson>& lessons, const std::string& id);
/**
 * @brief Splits a background into the prior condition and the following result.
 * 把背景分成前因和随后的结果。
 * @param text Background prose. A blank line is an explicit split. Without one, the split is the sentence that starts with 此后, 随后, 于是, 因此, or 所以, after at least two sentences.
 * 背景正文。空行是显式分界。没有空行时，在至少两句之后、以「此后」「随后」「于是」「因此」或「所以」开头的那一句分开。
 * @return The prior condition, then the result. The result is empty when the text has no such boundary.
 * 前因，然后是后果。没有这个分界时后果为空。
 */
std::pair<std::string, std::string> split_cause(const std::string& text);
/**
 * @brief Inserts a paragraph break after every two sentences.
 * 每两句插入一个段落空行。
 * @param text Background prose that has no causal boundary.
 * 没有因果分界的背景正文。
 * @return The same sentences, with a blank line after every second sentence when there are at least four.
 * 原句。至少四句时，每两句后有一个空行。
 */
std::string loosen_background(const std::string& text);
std::map<std::string, std::string> load_figures(const std::string& root);
LessonCopy load_briefs(const std::string& root);
const char* state_phrase(ExperimentState state);
int status_code(ExperimentState state);
int worse_status(int left, int right);
std::string basis_label(const std::string& basis);
std::string readme_text(const Manifest& manifest);
std::vector<std::string> stdout_files(const std::string& directory);
std::string latest_run_directory(const std::string& root, const std::string& id);
void emit_line(std::ostringstream* out, const std::string& text, bool html);
std::string readings_text(const std::string& run_directory, bool html);
bool tile_tokens(const std::string& line, std::vector<std::string>* tokens);
bool bits_only(const std::string& token);
std::string svg_text(int x, int y, const std::string& text, const char* fill, int size, const char* anchor);
std::string figure_html(const std::string& figure, const std::string& caption);
std::string paragraph_html(const std::string& text);
std::string disassembly_excerpt(const std::string& run_directory);
std::string page_shell(const std::string& title, const std::string& body);
bool is_source_name(const std::string& name);
std::string sources_html(const std::string& directory);
bool parse_integer(const std::string& text, long long* value);
std::string hex_digits(unsigned long long value, int width);
std::string byte_tiles(unsigned long long value);
std::string result_sentence(const std::string& status, const std::string& reason, bool proved);
void read_program_lines(const std::string& run_directory, std::vector<std::string>* steps,
                        std::vector<std::pair<std::string, std::string>>* metrics, std::string* status,
                        std::string* reason);
std::string demo_html(const std::string& lesson_id, const std::string& checks, const std::string& figure,
                      const std::vector<RunView>& runs, const std::string& latest, const std::string& run_token);
void remove_split_pages(const std::string& directory);
void write_course_page(const std::string& root, const std::vector<Lesson>& lessons, const Registry& registry,
                       const std::map<std::string, std::string>& figures, const LessonCopy& copy,
                       const std::string& fresh_id, const std::vector<RunView>& fresh_runs,
                       const std::string& run_token);
const char* phrase_for_code(int status);
void print_course(const std::vector<Lesson>& lessons);
void print_terminal(const std::string& root, const std::vector<Lesson>& lessons, std::size_t index,
                    const Registry& registry, const std::map<std::string, std::string>& figures,
                    const LessonCopy& copy, const std::vector<RunView>& runs);
std::size_t select_lesson(const std::string& root, const std::string& argument, const std::vector<Lesson>& lessons);
void remember_next(const std::string& root, std::size_t index, std::size_t count, int status);
int command_learn(const std::string& root, const std::string& argument, const SessionConfig& config, Registry& registry);
bool send_all(int fd, const char* data, std::size_t size);
std::string request_path(const std::string& request);
void reply_redirect(int client, const std::string& location);
void reply_html(int client, int status, const std::string& body, bool head_only);
std::string header_value(const std::string& request, const std::string& name);
bool host_allowed(const std::string& host, int port);
bool origin_allowed(const std::string& origin, int port);
std::string form_value(const std::string& body, const std::string& name);
std::string random_token();
void set_socket_timeouts(int fd);
bool read_request(int client, std::string* request);
std::string request_body(const std::string& request);
int command_web(const std::string& root, int port, const SessionConfig& config, Registry& registry);

}  // namespace syslab
