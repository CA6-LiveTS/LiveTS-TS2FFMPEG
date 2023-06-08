#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINUX_BUILD

int main(int argc, char *argv[]) {

    char cmd[1024] = {0};
    char line[128] = {0};
    char prev_line[128] = {0};

    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    long long int prev_milliseconds = 0;
    long long int curr_milliseconds = 0;

    int mode = 0; // 0 = Timestamps, 1 = Chapters, 2 = LiveTs Timestamps+Chapters


    if(argc != 4) {
        printf("Usage: %s <input mp4> <input LiveTS> <output mp4>\n", argv[0]);
        printf("WARNING: liveTs2Chapter use FFMPEG to encode the video with Chapters\n", argv[0]);
        return -1;
    }

    // open files
    FILE *inFile = fopen(argv[2], "r");
    FILE *outFile = fopen("chapter.tmp", "w");

    if (!inFile || !outFile) {
        printf("Error opening file!\n");
        return -2;
    }

    // Skip Video identification " ~ {video name} ~ {date} ~ {unix time}"
    fgets(line, sizeof(line), inFile); 

    // Skip Video URL "URL: {video url}"
    fgets(line, sizeof(line), inFile); 

    // TS identification "Timestamp:" or "Chapter:" or "LiveTs:" (mixed timestamp and chapter)
    fgets(line, sizeof(line), inFile); 

    // check is file contain chapters or timestamps
    if (strncmp(line, "Timestamp:", 10) == 0) {
        printf("File contain Timestamps\n");
        mode = 0;
    } else if (strncmp(line, "Chapter:", 8) == 0) {
        printf("File contain Chapters\n");
        mode = 1;
    } else if (strncmp(line, "LiveTs:", 7) == 0) {
        printf("File contain LiveTs Timestamps+Chapters\n");
        mode = 2;
    } else {
        printf("File contain unknown data\n");
        return -3;
    }

    // Handle the first line separately
    if (fgets(prev_line, sizeof(prev_line), inFile)) {
        sscanf(prev_line, "%d:%d:%d", &hours, &minutes, &seconds);
        prev_milliseconds = ((hours * 60 + minutes) * 60 + seconds) * 1000;
    }

    // Handle the rest of the lines
    while(fgets(line, sizeof(line), inFile)) {

        // if in LiveTS mode we ignore line that contain "@LiveTs" and "@TS" tags
        if (mode == 2 && (strstr(line, "@LiveTs") || strstr(line, "@TS"))) {
            continue;
        }

        sscanf(line, "%d:%d:%d", &hours, &minutes, &seconds);
        curr_milliseconds = ((hours * 60 + minutes) * 60 + seconds) * 1000;

        fprintf(outFile, "[CHAPTER]\nTIMEBASE=1/1000\nSTART=%lld\nEND=%lld\ntitle=%s\n", prev_milliseconds, curr_milliseconds-1, strchr(prev_line, ' ')+1);

        strcpy(prev_line, line);
        prev_milliseconds = curr_milliseconds;
    }

    // Print the last chapter without an end time
    fprintf(outFile, "[CHAPTER]\nTIMEBASE=1/1000\nSTART=%lld\ntitle=%s\n", prev_milliseconds, strchr(prev_line, ' ')+1);

    // close the file
    fclose(inFile);
    fclose(outFile);

#ifdef LINUX_BUILD
    // extract metadata from video
    snprintf(cmd, sizeof(cmd), "ffmpeg -i %s -f ffmetadata meta.tmp", argv[1]);
    system(cmd);

    // merge metadata and chapter
    snprintf(cmd, sizeof(cmd), "cat meta.tmp chapter.tmp > meta2.tmp");
    system(cmd);

    // add metadata to video
    snprintf(cmd, sizeof(cmd), "ffmpeg -i %s -i meta2.tmp -map_metadata 1 -codec copy -f mp4 -y %s", argv[1], argv[3]);
    system(cmd);

    // delete temp files
    snprintf(cmd, sizeof(cmd), "rm meta.tmp meta2.tmp chapter.tmp");
    system(cmd);
#else
    printf("please run ffmpeg encoding on the file, automated mp4 encoding only supported on linux\n");
    printf("extract metadata from video with: ffmpeg -i %s -f ffmetadata meta.tmp", argv[1]);
    printf("append chapter.tmp at the end of extracted meta.tmp", argv[1]);
    printf("create new video with chapter: ffmpeg -i %s -i meta2.tmp -map_metadata 1 -codec copy -f mp4 -y %s", argv[1], argv[3]);
#endif

/*
ffmpeg -i INPUT.mp4 -f ffmetadata meta.tmp
cat meta.tmp chapter.tmp > meta2.tmp
ffmpeg -i INPUT.mp4 -i meta2.tmp -map_metadata 1 -codec copy OUTPUT.mp4
*/

    return 0;
}
