# 매크로 설정
CC = gcc
CFLAGS = -Wall -Wextra -O2
LIBS = -lcrypto           # MD5 해시를 위한 라이브러리 링크 추가
TARGET = sync             # 최종 생성될 메인 실행 파일명
DAEMON_TARGET = daemon    # 데몬 실행 파일명

# 소스 파일 및 오브젝트 파일 목록
SRCS = main.c utils.c add.c remove.c list.c help.c
OBJS = $(SRCS:.c=.o)

DAEMON_SRCS = daemon.c
DAEMON_OBJS = $(DAEMON_SRCS:.c=.o)

# 기본 타겟: sync와 daemon을 모두 빌드
all: $(TARGET) $(DAEMON_TARGET)

# sync 빌드
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# daemon 빌드
$(DAEMON_TARGET): $(DAEMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# 각 .c 파일을 .o 파일로 컴파일
%.o: %.c sync.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(DAEMON_OBJS) $(DAEMON_TARGET)

# 의존성 라인 (헤더 파일 변경 시 전체 재컴파일 보장)
$(OBJS) $(DAEMON_OBJS): sync.h
