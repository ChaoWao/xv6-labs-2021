	.text

	/*
         * save the old thread's registers,
         * restore the new thread's registers.
         */

	.globl thread_switch
/*
 * parameter:
 *   a0: next_thread's stack's lowest address
 *   a1: curr_thread's stack's lowest address
 * return:
 *   none
 */
thread_switch:
	/* YOUR CODE HERE */
	/* caller saved register shall be handled by caller (thread_schedule)
	 * we need to handle caller saved regester
	 * they are sp, s0, s1, s2-s11
	 * we store sp at fixed address, its stack's lowest position
	 * so that they can be fectched through parameter
	 */
	addi sp, sp, -104
	sd ra, 0(sp)
	sd s0, 8(sp)
	sd s1, 16(sp)
	sd s2, 24(sp)
	sd s3, 32(sp)
	sd s4, 40(sp)
	sd s5, 48(sp)
	sd s6, 56(sp)
	sd s7, 64(sp)
	sd s8, 72(sp)
	sd s9, 80(sp)
	sd s10, 88(sp)
	sd s11, 96(sp)
	sd sp, 0(a1)
	/* restore next_thread's register just do the oppisite */
	ld sp, 0(a0)
	ld ra, 0(sp)
	ld s0, 8(sp)
	ld s1, 16(sp)
	ld s2, 24(sp)
	ld s3, 32(sp)
	ld s4, 40(sp)
	ld s5, 48(sp)
	ld s6, 56(sp)
	ld s7, 64(sp)
	ld s8, 72(sp)
	ld s9, 80(sp)
	ld s10, 88(sp)
	ld s11, 96(sp)
	addi sp, sp, 104
	ret    /* return to ra */
