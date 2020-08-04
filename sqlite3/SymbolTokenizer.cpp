#include "sqlite3ext.h"
#include <string_view>
#include <iostream>

SQLITE_EXTENSION_INIT1

static fts5_api *fts5_api_from_db(sqlite3 *db) {
  fts5_api *pRet      = 0;
  sqlite3_stmt *pStmt = 0;

  if (SQLITE_OK == sqlite3_prepare(db, "SELECT fts5(?1)", -1, &pStmt, 0)) {
    sqlite3_bind_pointer(pStmt, 1, (void *) &pRet, "fts5_api_ptr", NULL);
    sqlite3_step(pStmt);
  }
  sqlite3_finalize(pStmt);
  return pRet;
}

static fts5_api *fts5 = nullptr;

static int symbol_xCreate(void *, const char **azArg, int nArg, Fts5Tokenizer **ppOut) {
  *ppOut = (Fts5Tokenizer *) "fake";
  return SQLITE_OK;
}
static void symbol_xDelete(Fts5Tokenizer *self) {}

inline bool isTokenChar(char ch) { return ch == '$' || ch == '_' || 'a' <= ch && ch <= 'z' || 'A' <= ch && ch <= 'Z'; }

inline bool isAlpha(char ch) { return 'a' <= ch && ch <= 'z' || 'A' <= ch && ch <= 'Z'; }

inline bool isUpperCase(char ch) { return 'A' <= ch && ch <= 'Z'; }

inline bool isNum(char ch) { return '0' <= ch && ch <= '9'; }

inline bool isSep(char ch) { return !isTokenChar(ch) && !isNum(ch); }

#if 1 && !defined(NDEBUG)
#  define iemit oemit
#  define DEBUG_EMIT
#else
#  define iemit emit
#endif

static int symbol_tokenize(
    Fts5Tokenizer *, void *ctx, int flags, const char *text, int len,
    int (*iemit)(void *pCtx, int tflags, const char *pToken, int nToken, int iStart, int iEnd)) {
  int rc = SQLITE_OK;
  if (text == NULL) return rc;
  int rec0 = 0, rec1 = 0;
  enum struct State { None, Alpha, Number, Seprator, Operator } state{};

#ifdef DEBUG_EMIT
  auto emit = [&](void *pCtx, int tflags, const char *pToken, int nToken, int iStart, int iEnd) -> int {
    std::cout << "token " << (tflags ? "-" : "+") << " (" << std::string_view{pToken, (size_t) nToken} << "), "
              << iStart << ", " << iEnd << std::endl;
    return oemit(pCtx, tflags, pToken, nToken, iStart, iEnd);
  };
#endif

  for (int i = 0; i < len; i++) {
    auto const &cur = text[i];
    switch (state) {
    case State::None:
      rec0 = rec1 = i;
      if (isTokenChar(cur))
        state = State::Alpha;
      else if (isNum(cur))
        state = State::Number;
      break;
    case State::Alpha:
      if (isSep(cur)) {
        state = cur == ':' ? State::Seprator : State::None;
        emit(ctx, 0, text + rec0, i - rec0, rec0, i);
        if (rec0 != rec1)
          emit(ctx, 0, text + rec1, i - rec1, rec1, i);
        else if (std::string_view{text + rec0, (size_t) i - rec0} == "operator") {
          rec1  = i;
          state = State::Operator;
        }
        rec0 = i;
      } else if (isUpperCase(cur) || isNum(cur)) {
        emit(ctx, 0, text + rec1, i - rec1, rec1, i);
        rec1 = i;
      } else if (cur == '_' && text[rec0] != '$') {
        emit(ctx, 0, text + rec1, i - rec1, rec1, i);
        rec1 = ++i;
      }
      break;
    case State::Number:
      if (isNum(cur)) continue;
      if (isTokenChar(cur)) {
        emit(ctx, 0, text + rec1, i - rec1, rec1, i);
        rec1  = i;
        state = State::Alpha;
      } else if (isSep(cur)) {
        emit(ctx, 0, text + rec0, i - rec0, rec0, i);
        if (rec0 != rec1) emit(ctx, 0, text + rec1, i - rec1, rec1, i);
        state = cur == ':' ? State::Seprator : State::None;
        rec0  = i;
      }
      break;
    case State::Seprator:
      if (cur == ':') emit(ctx, 0, text + rec0, i - rec0 + 1, rec0, i + 1);
      state = State::None;
      break;
    case State::Operator:
      if (cur == '(') {
        emit(ctx, 0, text + rec0, i - rec0, rec0, i);
        state = State::None;
      }
      break;
    default: return SQLITE_MISUSE;
    }
  }
  switch (state) {
  case State::None: break;
  case State::Alpha:
  case State::Number:
    emit(ctx, 0, text + rec0, len - rec0, rec0, len);
    if (rec0 != rec1) emit(ctx, 0, text + rec1, len - rec1, rec1, len);
    break;
  case State::Operator: emit(ctx, 0, text + rec0, len - rec0, rec0, len); break;
  default: break;
  }
  return rc;
}

static fts5_tokenizer tokenizer{
    .xCreate   = symbol_xCreate,
    .xDelete   = symbol_xDelete,
    .xTokenize = symbol_tokenize,
};

void symprefix(sqlite3_context *ctx, int, sqlite3_value **values) {
  auto str = sqlite3_value_text(values[0]);
  auto len = sqlite3_value_bytes(values[0]);

  std::string buf;
  buf.reserve(len);
  auto level = 0, skiplvl = 0;

  for (int i = 0; i < len; i++) {
    auto cur = str[i];
    if (skiplvl == 0) {
      if (cur == '<') {
        buf += ' ';
        level++;
      } else if (cur == '>') {
        buf += ' ';
        level--;
      } else if (cur == '(') {
        if (buf.ends_with("operator"))
          buf += cur;
        else if (level == 0)
          break;
        skiplvl++;
      } else if (cur != '-'){
        buf += cur;
      }
    } else {
      if (cur == '(' && !buf.ends_with("operator")) {
        skiplvl++;
      } else if (cur == ')' && !buf.ends_with("operator(")) {
        skiplvl--;
      }
    }
  }
  sqlite3_result_text(ctx, buf.c_str(), (int) buf.size(), SQLITE_TRANSIENT);
}

extern "C" __declspec(dllexport) int sqlite3_symboltokenizer_init(
    sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi) {
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
  fts5 = fts5_api_from_db(db);
  fts5->xCreateTokenizer(fts5, "symbol", nullptr, &tokenizer, nullptr);
  sqlite3_create_function(db, "symprefix", 1, SQLITE_UTF8 | SQLITE_DIRECTONLY, nullptr, symprefix, nullptr, nullptr);
  return rc;
}