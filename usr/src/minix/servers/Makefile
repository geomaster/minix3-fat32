.include <bsd.own.mk>

SUBDIR+=	ds input pm rs sched vfs vm fat32

.if ${MKIMAGEONLY} == "no"
SUBDIR+=	ipc is devman
.endif

.include <bsd.subdir.mk>
