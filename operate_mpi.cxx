#include "operate_mpi.h"
#include "variable.h"
#ifdef _MPI
int Set_MPI_initalize (void) {
    int *recvtag, *sendtag;
    int diameter = 0;

    MPI_Comm_size (MPI_COMM_WORLD, &procs);
    MPI_Comm_rank (MPI_COMM_WORLD, &procid);

// MPI Parameter Setting
    xprocs = procs;
    yprocs = procs;
    for (int i = 0; i < xprocs; i++) {
        for (int j = 0; j < yprocs; j++) {
            if(procid == (i * yprocs + j)) {
                xid = i; yid =j;
            }
        }
    }
// Make MPI send/recv tag
    tags = alloc_2d_int (procs + 1, procs + 1);
    sendtag = alloc_1d_int (procs + 1);
    recvtag = alloc_1d_int (procs + 1);
    for (int i = 0; i <= procs; i++) {
        sendtag[i] = i;
        recvtag[i] = i;
    }
    if (procs > 0 && procs < 10) {
        diameter = 10;
    } else if (procs > 10 && procs < 100) {
        diameter = 100;
    } else if (procs > 100 && procs < 1000) {
        diameter = 1000;
    }
    for (int i = 0; i <= procs; i++) {
        for (int j = 0; j <= procs; j++) {
            tags[i][j] = sendtag[i] * diameter + recvtag[j];
        }
    }
    free_1d_int (sendtag);
    free_1d_int (recvtag);
// Setting MPI send/recv status array and request array
    ireq = (MPI_Request *) malloc (2 * procs * sizeof (MPI_Request) );
    alloc_error_check (ireq);
    ista = (MPI_Status *) malloc (2 * procs * sizeof (MPI_Status) );
    alloc_error_check (ista);
    MPI_ERRORCHECK (ierr);
    return ierr;
}
int Set_MPI_finalize (void) {
    ierr = MPI_Comm_free(&OWN_X_COMM);
    MPI_ERRORCHECK (ierr);
    ierr = MPI_Group_free(&OWN_X_GROUP);
    MPI_ERRORCHECK (ierr);
    ierr = MPI_Comm_free(&OWN_Y_COMM);
    MPI_ERRORCHECK (ierr);
    ierr = MPI_Group_free(&OWN_Y_GROUP);
    MPI_ERRORCHECK (ierr);
    ierr = MPI_Group_free(&GROUP_WORLD);
    MPI_ERRORCHECK (ierr);
    free_1d_int(xmesh);
    free_1d_int(ymesh);
    free (ireq);
    free (ista);
    free_2d_int (tags);
    free_2d_int (idtbl);
    free_2d_int (lj_ref);
    free_2d_int (sekibun_ref);
    MPI_Finalize();
    ierr = 0;
    return ierr;
}
void Set_ID_Table (void) {
    const double LJ_cutoff = A_R_cutoff * LJ_dia;
    const int dmy_lj = (int) lrint (ceil(LJ_cutoff * IDX));
    const int dmy_sekibun = Max_Sekibun_cell;
    double x, y, offset;
    double inpx = 0.0;
    double inqy = 0.0;
    int nps_min[SPACE][DIM];
    int *chk, **dmytbl;
    int id, dmy_x, dmy_y, offset_2x, offset_2y, xy;
    int *npx_all, *nqy_all;
    int *xgroup;
    int *ygroup;

    for (int i = 0; i < SPACE; i++){
        for (int j = 0; j < DIM; j++){
            nps_min[i][j] = INT_MAX;
        }
    }
    for (int i = 0; i < procs; i++){
        for (int j = 0; j < SPACE; j++){
            for (int k = 0; k < DIM; k++){
                if (nps_min[j][k] > NPs_ALL[(i * SPACE * DIM) + (j * DIM) + k]) {
                    nps_min[j][k] = NPs_ALL[(i * SPACE * DIM) + (j * DIM) + k];
                }
            }
        }
    }
    inpx = (1.0 / nps_min[REAL][0]);
    inqy = (1.0 / nps_min[REAL][1]);
    // Split 2D
    if (dmy_lj >= dmy_sekibun) {
        offset = dmy_lj;
    } else {
        offset = dmy_sekibun;
    }
    offset_x = (int) lrint (ceil (offset * inpx)) + 1;
    offset_y = (int) lrint (ceil (offset * inqy)) + 1;
// Make ID table
    offset_2x = 2 * offset_x;
    offset_2y = 2 * offset_y;
    idtbl = calloc_2d_int ( xprocs + offset_2x, yprocs + offset_2y );
    //ID�e�[�u���쐬�F�������E�������l�����āAID�e�[�u���擾�}�N���̈�����
    //                ���l�����͂����O��ō쐬����ׁA�v���Z�X�i�q�}�I�t�Z�b�g�l���g�p
    for (int xp = 0; xp < xprocs + offset_2x; xp++) {
        dmy_x = (xp + (xprocs - offset_x) ) % xprocs;
        for (int yp = 0; yp < yprocs + offset_2y; yp++) {
            dmy_y = (yp + (yprocs - offset_y) ) % yprocs;
            idtbl[xp][yp] = dmy_x * yprocs + dmy_y;
        }
    }
    //���e�[�u���쐬(LJ_cutoff�p)
    //�����_(0,0)�͎��v���Z�X���̊i�q�_�Ƃ���
    lj_ref = calloc_2d_int ((4 * (dmy_lj + 1) * (dmy_lj + 1)), 2);
    xy = 0;
    for (int xp = -HNX; xp <= HNX; xp++) {
        for (int yp = -HNY; yp <= HNY; yp++) {
            //LJ�J�b�g�I�t�����𔼌a�Ƃ��Č��_����i�q�_�܂ł̋�Ԃ�
            //���̋��E�����ɂ���Ȃ�ΐ^
            if((xp * xp) + (yp * yp) <= (dmy_lj * dmy_lj)) {
                //�i�q���W����A���΃v���Z�X���W�����߂�
                x = xp * inpx;
                y = yp * inqy;
                lj_ref[xy][0] = (x > 0) ? lrint (ceil(x)) : lrint (floor(x));
                lj_ref[xy][1] = (y > 0) ? lrint (ceil(y)) : lrint (floor(y));
/*
                //���S�̂��ߕ��������_���l�����Ĕ�����s��
                if(x < DBL_EPSILON && x > -DBL_EPSILON) {
                    lj_ref[xy][0] = 0;
                } else if(x >= DBL_EPSILON ) {
                    lj_ref[xy][0] = (int) lrint (ceil(x));
                } else if(x <= -DBL_EPSILON) {
                    lj_ref[xy][0] = (int) lrint (floor(x));
                }
                if(y < DBL_EPSILON && y > -DBL_EPSILON) {
                    lj_ref[xy][0] = 0;
                } else if(y >= DBL_EPSILON ) {
                    lj_ref[xy][0] = (int) lrint (ceil(y));
                } else if(y <= -DBL_EPSILON) {
                    lj_ref[xy][0] = (int) lrint (floor(y));
                }
*/
                xy++;
            }
        }
    }
    //�e�[�u���̏d��ID�폜
    lj_size = xy;
    chk = calloc_1d_int(procs);
    dmytbl = alloc_2d_int(lj_size + 1, 2);
    //���������_�̌덷�΍�
    chk[0]++;
    dmytbl[0][0] = 0;
    dmytbl[0][1] = 0;
    xy = 1;
    for (int i = 0; i < lj_size; i++) {
        //�d�������̂��߉���X�����v���Z�XID=0,Y�����v���Z�XID=0�Ɖ��肵��
        //���e�[�u����ɍ쐬�������΃v���Z�X���W������ID�e�[�u������l�����o���Ă���
        id = ID(lj_ref[i][0], lj_ref[i][1]);
        //�d�����Ă��Ȃ��ꍇ�A���e�[�u���̑��΃v���Z�X���W�������A
        //�`�F�b�N�ς݃J�E���^��1���Z
        if (chk[id] == 0) {
            dmytbl[xy][0] = lj_ref[i][0];
            dmytbl[xy][1] = lj_ref[i][1];
            chk[id]++;
            xy++;
        }
    }
    free_1d_int(chk);
    free_2d_int(lj_ref);
    //�e�[�u���쐬(LJ_cutoff�p)
    lj_size = xy;
    lj_ref = calloc_2d_int(procs, lj_size);
    for (int xp = 0; xp < xprocs; xp++) {
        for (int yp = 0; yp < yprocs; yp++) {
            //X�����v���Z�XID, Y�����v���Z�XID���߁A���ۂ̃v���Z�XID�����߂�
            id = ID(xp, yp);
            //��߂��v���Z�XID����A���e�[�u����ɍ쐬�������΃v���Z�X���W������
            //��߂��v���Z�XID�̎��͂Ɉʒu����v���Z�XID�����߃e�[�u���ɑ������
            for (int i = 0; i < lj_size; i++) {
                lj_ref[id][i] = ID(xp + dmytbl[i][0], yp + dmytbl[i][1]);
            }
        }
    }
    free_2d_int(dmytbl);
    //���e�[�u���쐬(Sekibun_cell�p)
    //�����_(0,0)�͎��v���Z�X���̊i�q�_�Ƃ���
    sekibun_ref = calloc_2d_int ((4 * (dmy_sekibun + 1) * (dmy_sekibun + 1)), 2);
    xy = 0;
    for (int xp = -HNX; xp <= HNX; xp++) {
        for (int yp = -HNY; yp <= HNY; yp++) {
            //�v���t�@�C���֐����z���b�V���̍ő啝�𔼌a�Ƃ��Č��_����i�q�_�܂ł̋�Ԃ�
            //���̋��E�����ɂ���Ȃ�ΐ^
            if((xp * xp) + (yp * yp) <= (dmy_sekibun * dmy_sekibun)) {
                //�i�q�_�̑��΍��W����A���΃v���Z�X���W�����߂�
                x = xp * inpx;
                y = yp * inqy;
                sekibun_ref[xy][0] = (x > 0.0) ? lrint (ceil(x)) : lrint (floor(x));
                sekibun_ref[xy][1] = (y > 0.0) ? lrint (ceil(y)) : lrint (floor(y));
/*
                //���S�̂��ߕ��������_���l�����Ĕ�����s��
                if(x < DBL_EPSILON && x > -DBL_EPSILON) {
                    sekibun_ref[xy][0] = 0;
                } else if(x >= DBL_EPSILON ) {
                    sekibun_ref[xy][0] = (int) lrint (ceil(x));
                } else if(x <= -DBL_EPSILON) {
                    sekibun_ref[xy][0] = (int) lrint (floor(x));
                }
                if(y < DBL_EPSILON && y > -DBL_EPSILON) {
                    sekibun_ref[xy][0] = 0;
                } else if(y >= DBL_EPSILON ) {
                    sekibun_ref[xy][0] = (int) lrint (ceil(y));
                } else if(y <= -DBL_EPSILON) {
                    sekibun_ref[xy][0] = (int) lrint (floor(y));
                }
*/
                xy++;
            }
        }
    }
    //�e�[�u���̏d��ID�폜
    sekibun_size = xy;
    chk = calloc_1d_int(procs);
    dmytbl = alloc_2d_int(sekibun_size + 1, 2);
    //���������_�̌덷�΍�
    chk[0]++;
    dmytbl[0][0] = 0;
    dmytbl[0][1] = 0;
    xy = 1;
    for (int i = 0; i < sekibun_size; i++) {
        //�d�������̂��߉���X�����v���Z�XID=0,Y�����v���Z�XID=0�Ɖ��肵��
        //���e�[�u����ɍ쐬�������΃v���Z�X���W������ID�e�[�u������l�����o���Ă���
        id = ID(sekibun_ref[i][0], sekibun_ref[i][1]);
        //�d�����Ă��Ȃ��ꍇ�A���e�[�u���̑��΃v���Z�X���W�������A
        //�`�F�b�N�ς݃J�E���^��1���Z
        if (chk[id] == 0) {
            dmytbl[xy][0] = sekibun_ref[i][0];
            dmytbl[xy][1] = sekibun_ref[i][1];
            chk[id]++;
            xy++;
        }
    }
    free_1d_int(chk);
    free_2d_int(sekibun_ref);
    //�e�[�u���쐬(Sekibun_cell�p)
    sekibun_size = xy;
    sekibun_ref = calloc_2d_int(procs, sekibun_size);
    for (int xp = 0; xp < xprocs; xp++) {
        for (int yp = 0; yp < yprocs; yp++) {
            //X�����v���Z�XID, Y�����v���Z�XID���߁A���ۂ̃v���Z�XID�����߂�
            id = ID(xp, yp);
            //��߂��v���Z�XID����A���e�[�u����ɍ쐬�������΃v���Z�X���W������
            //��߂��v���Z�XID�̎��͂Ɉʒu����v���Z�XID�����߃e�[�u���ɑ������
            for (int i = 0; i < sekibun_size; i++) {
                sekibun_ref[id][i] = ID(xp + dmytbl[i][0], yp + dmytbl[i][1]);
            }
        }
    }
    free_2d_int(dmytbl);
    if(procid == root) {
        x = dmy_lj * inpx;
        y = dmy_lj * inqy;
        //tablesize�̏o�͂Ƃ��Ă͒����v���Z�X���܂߂��l�ɂȂ�
        fprintf(stderr, "# LJ_cutoff - Mesh width = %d, LJ tablesize = %d (%d, %d)\n",
                dmy_lj, lj_size, (int)lrint(ceil(x)), (int)lrint(ceil(y)));
        x = dmy_sekibun * inpx;
        y = dmy_sekibun * inqy;
        fprintf(stderr, "# Sekibun_cell - Mesh width = %d, Sekibun_cell tablesize = %d (%d, %d)\n",
                dmy_sekibun, sekibun_size, (int)lrint(ceil(x)), (int)lrint(ceil(y)));
    }
    // ���M��ID
    xmesh = alloc_1d_int(NX);
    ymesh = alloc_1d_int(NY);
    npx_all = calloc_1d_int(xprocs + 1);
    nqy_all = calloc_1d_int(yprocs + 1);
    dmy_x = dmy_y = 0;
    for (int xp = 0; xp <= xprocs; xp++){
        npx_all[xp + 1] = npx_all[xp] + NPs_ALL[((xp * yprocs + 0) * SPACE * DIM) + (REAL * DIM) + 0];
    }
    for (int yp = 0; yp < yprocs; yp++){
        nqy_all[yp + 1] = nqy_all[yp] + NPs_ALL[((0 * yprocs + yp) *SPACE * DIM) + (REAL * DIM) + 1];
    }
    for (int i = 0; i < NX; i++) {
        if(npx_all[dmy_x] <= i && i < npx_all[dmy_x + 1]) {
             xmesh[i] = dmy_x;
        } else {
             //��L�������U�������ꍇ�Admy_x���C���N�������g����
             //���[�v�J�E���g��߂�
             ++dmy_x; --i;
        }
    }
    for (int i = 0; i < NY; i++) {
        if(nqy_all[dmy_y] <= i && i < nqy_all[dmy_y + 1]) {
             ymesh[i] = dmy_y;
        } else {
             //��L�������U�������ꍇ�Admy_y���C���N�������g����
             //���[�v�J�E���g��߂�
             ++dmy_y; --i;
        }
    }
    free_1d_int(npx_all);
    free_1d_int(nqy_all);
    // Comm Group
    xgroup = alloc_1d_int(xprocs);
    ygroup = alloc_1d_int(yprocs);
    for (int xp = 0; xp < xprocs; xp++) {
        xgroup[xp] = ID (xp, yid);
    }
    for (int yp = 0; yp < yprocs; yp++) {
        ygroup[yp] = ID (xid, yp);
    }
    ierr = MPI_Comm_group(MPI_COMM_WORLD, &GROUP_WORLD);
    MPI_ERRORCHECK (ierr);
    ierr = MPI_Group_incl (GROUP_WORLD, xprocs, xgroup, &OWN_X_GROUP);
    MPI_ERRORCHECK (ierr);
    ierr = MPI_Comm_create (MPI_COMM_WORLD, OWN_X_GROUP, &OWN_X_COMM);
    MPI_ERRORCHECK (ierr);
    ierr = MPI_Group_incl (GROUP_WORLD, yprocs, ygroup, &OWN_Y_GROUP);
    MPI_ERRORCHECK (ierr);
    ierr = MPI_Comm_create (MPI_COMM_WORLD, OWN_Y_GROUP, &OWN_Y_COMM);
    MPI_ERRORCHECK (ierr);
    free_1d_int(xgroup);
    free_1d_int(ygroup);

}
// Data Gathering
void Get_mesh_array (double *src, double *dist, enum SW_SWITCH sw) {
    const int scount = NPs[REAL][0] * NPs[REAL][1] * NZ_;

    if (sw == SW_OFF) {
        ierr = MPI_Gatherv (src, scount, MPI_DOUBLE, dist, rcounts, displs,
                            MPI_DOUBLE, root, MPI_COMM_WORLD);
    } else {
        ierr = MPI_Allgatherv (src, scount, MPI_DOUBLE, dist, rcounts, displs,
                               MPI_DOUBLE, MPI_COMM_WORLD);
    }

    MPI_ERRORCHECK (ierr);
}
// Data Scattering
void Set_mesh_array (double *dist, double *src) {
    int i, j, k;
    int g_im, l_im;

    for (i = 0; i < NPs[REAL][0]; i++) {
        for (j = 0; j < NPs[REAL][1]; j++) {
            for (k = 0; k < NPs[REAL][2]; k++) {
                g_im = ((i + PREV_NPs[REAL][0]) * NY * NZ_) + ((j + PREV_NPs[REAL][1]) * NZ_) + (k + PREV_NPs[REAL][2]);
                l_im = REALMODE_ARRAYINDEX(i, j, k);
                dist[l_im] = src[g_im];
            }
        }
    }

}
#endif /* _MPI */
