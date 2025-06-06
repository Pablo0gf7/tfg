#ifndef QIM_H
#define QIM_H

void embed_message(const char *infile, const char *outfile, const char *message);
void extract_message(const char *infile, int msg_bytes);

#endif // QIM_H