#include "Utils.h"
#include "Backend.h"
#include "Protocol.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char* trim_inplace(char* s) {
  if (!s) return s;
  while (*s && isspace((unsigned char)*s)) s++;
  if (!*s) return s;
  char* end = s + strlen(s) - 1;
  while (end > s && isspace((unsigned char)*end)) {
    *end = '\0';
    end--;
  }
  return s;
}

int parse_int(const char* s) {
  if (!s) return 0;
  return atoi(s);
}

bool iequals(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    a++;
    b++;
  }
  return *a == '\0' && *b == '\0';
}

void sendAck(uint8_t seq, bool ok, uint8_t code) {
  uint8_t ack[6] = {
    STX,
    VERSION,
    seq,
    static_cast<uint8_t>(ok ? 1 : 0),
    code,
    ETX,
  };
  backend_send_bytes(ack, sizeof(ack));
}
