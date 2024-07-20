#ifdef __cplusplus
extern "C" {
#endif

char *serialize_simple_string(const char *str);
char *serialize_error(const char *str);
char *serialize_integer(const int val);
char *serialize_bulk_string(const char *str);
char *serialize_array(const char **arr, int count);

#ifdef __cplusplus
}
#endif