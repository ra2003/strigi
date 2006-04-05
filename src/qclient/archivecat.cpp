#include "archiveenginehandler.h"
int
main(int argc, char** argv) {
    ArchiveEngineHandler engine;
    for (int i=1; i<argc; ++i) {
        QFile file(argv[i]);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug("Could not open '%s'\n", argv[i]);
            return 1;
        }
        const int bufsize=10;
        char buf[bufsize];
        qint64 read = file.read(buf, bufsize);
        while (read > 0) {
            fwrite(buf, read, 1, stdout);
            read = file.read(buf, bufsize);
        }
    }
	return 0;
}
