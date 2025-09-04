#ifndef QIM_H
#define QIM_H

void embed_message(const char *infile, const char *outfile, const char *message, const char *password);
void extract_message(const char *infile, const char *password);
void metrics(const char *orig_file, const char *stego_file, const char *out_txt, long samples_opt);

#endif