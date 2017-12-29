#include <stdio.h>

FILE *I,*O;

void main()
{
  int d, count = 0;
  FILE *I = fopen("C:\\temp\\out13.bin", "rb");
  FILE *O = fopen("C:\\temp\\out.c", "w");

  while ((d = fgetc(I)) != -1)
  {
    fprintf(O, "memoryDmemSetByte(0x%.2X); ", d);
    count++;
    if ((count % 4) == 0)
    {
      fprintf(O, "\n");
    }
  }
  fclose(O);
  fclose(I);
  return 0;
}
