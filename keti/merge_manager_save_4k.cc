#include "merge_manager.h"

void MergeManager::Merging(){
    // key_t key1=54321;
    // int msqid;
    // message msg;
    // msg.msg_type=1;
    // if((msqid=msgget(key1,IPC_CREAT|0666))==-1){
    //     printf("msgget failed\n");
    //     exit(0);
    // }
    // printf("~MergeBlock~ # workid: %d, blockid: %d, rows: %d, length: %d, offset_len: %ld\n",result.work_id, result.block_id, result.rows, result.totallength, result.offset.size());
    // string rowfilter = "<-----------  Merge Manager Running...  ----------->";
    // strcpy(msg.msg,rowfilter.c_str());
    // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
    //     printf("msgsnd failed\n");
    //     exit(0);
    // }

    while (1){
        Result result = MergeQueue.wait_and_pop();
        // for(int i = 0; i < result.offset.size(); i++){
        //     cout << result.offset[i] << endl;
        // }
        // cout << "\n--------------------merge--------------------------\n";
        // for(int i = 0; i < result.length;i++){
        //     printf("%02X",(u_char)result.data[i]);
        // }
        // cout << "\n----------------------------------------------\n";
        // string rowfilter = "<------------Merge Block------------>";
        // strcpy(msg.msg,rowfilter.c_str());
        // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
        //     printf("msgsnd failed\n");
        //     exit(0);
        // }
                
        MergeBlock(result);
    }
}

void MergeManager::MergeBlock(Result &result){
    // cout << result.data << endl;
    // printf("~MergeBlock~ # workid: %d, blockid: %d, rows: %d, length: %d, offset_len: %ld\n",result.work_id, result.block_id, result.rows, result.totallength, result.offset.size());

    // key_t key1=54321;
    // int msqid;
    // message msg;
    // msg.msg_type=1;
    // if((msqid=msgget(key1,IPC_CREAT|0666))==-1){
    //     printf("msgget failed\n");
    //     exit(0);
    // }
    // string rowfilter = "[Get Block Filter Result]\n";
    // rowfilter += "-------------------------------Filtered Block Info----------------------------------\n";
    // rowfilter += "| work id: " + to_string(result.work_id)+ " | block id: " + to_string(result.block_id) 
    //           + " | rows: " + to_string(result.rows) + " | filtered block length: " + to_string(result.totallength)
    //           + " | is last block: " + to_string(result.is_last_block) + " | \n";
    // rowfilter += "------------------------------------------------------------------------------------\n";
    
    // strcpy(msg.msg,rowfilter.c_str());
    // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
    //     printf("msgsnd failed\n");
    //     exit(0);
    // }
    
    // ostringstream oss;
    // for(int i = 0; i < result.totallength; i++){
    //     cout << hex << (int)result.data[i];
    // }

    // strcpy(msg.msg,oss.str().c_str());
    // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
    //     printf("msgsnd failed\n");
    //     exit(0);
    // }

    // rowfilter = "------------------------------------------------------------------------------------\n";

    // strcpy(msg.msg,rowfilter.c_str());
    // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
    //     printf("msgsnd failed\n");
    //     exit(0);
    // }
	    
    int row_len = 0;
    int row_num = 0;
    int block_size = 0;
    int key = result.work_id;

    if(m_MergeManager.find(key)==m_MergeManager.end()){//Key에 해당하는 블록버퍼가 없다면 생성
        MergeResult block(key, result.csd_name, result.total_block_count);
        m_MergeManager.insert(make_pair(key,block));
    } 
    
    if(result.row_count == 0){//현재 블록 데이터가 모두 필터되었다면
        // string rowfilter = "**current filtered block {" + to_string(result.block_id) + "} length is 0\n";
        // strcpy(msg.msg,rowfilter.c_str());
        // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
        //     printf("msgsnd failed\n");
        //     exit(0);
        // }
        m_MergeManager[key].block_id_list.push_back(make_tuple(result.block_id,-1,0,true));
                
        //return;
    }

    else{// 블록에 데이터가 있다면
        // rowfilter = "\n[Merge Filtered Block Rows...]\n";
        // strcpy(msg.msg,rowfilter.c_str());
        // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
        //     printf("msgsnd failed\n");
        //     exit(0);
        // }

        vector<int> temp_offset;
        temp_offset.assign( result.row_offset.begin(), result.row_offset.end() );
        temp_offset.push_back(result.length);
        int block_start_offset = m_MergeManager[key].length;

        //새 블록 넣기 전 확인
        row_len = temp_offset[1] - temp_offset[0];

        if(m_MergeManager[key].length + row_len > BUFF_SIZE){//row추가시 데이터 크기 넘으면
            // rowfilter = "\n[Send Merged Block Data To Buffer Manager]";
            // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
            //     printf("msgsnd failed\n");
            //     exit(0);
            // }       
            ReturnQueue.push_work(m_MergeManager[key]);
            m_MergeManager[key].InitMergeResult();
            block_start_offset = 0;
        }

        //새 블록 넣기
        for(int i=0; i<result.row_count; i++){
            row_len = temp_offset[i+1] - temp_offset[i];
            if(m_MergeManager[key].length + row_len > BUFF_SIZE){//row추가시 데이터 크기 넘으면
                block_size = m_MergeManager[key].length - block_start_offset;
                m_MergeManager[key].block_id_list.push_back(make_tuple(result.block_id,block_start_offset,block_size,false));
                // rowfilter = "\n[Send Merged Block Data To Buffer Manager]";
                // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
                //     printf("msgsnd failed\n");
                //     exit(0);
                // }           
                ReturnQueue.push_work(m_MergeManager[key]);
                m_MergeManager[key].InitMergeResult();
                block_start_offset = 0;
            }

            m_MergeManager[key].row_offset.push_back(m_MergeManager[key].length); //현재 row 시작 offset
            m_MergeManager[key].rows += 1;
            int current_offset = m_MergeManager[key].length;
            // memcpy(m_MergeManager[key].data+current_offset, result.data+temp_offset[i], row_len);
            for(int j = temp_offset[i]; j<temp_offset[i+1]; j++){
                m_MergeManager[key].data[current_offset] =  result.data[j]; // 현재 row데이터 복사
                current_offset += 1;
            }
            m_MergeManager[key].length += row_len;// 데이터 길이 = row 전체 길이
        }

        block_size = m_MergeManager[key].length - block_start_offset;
        m_MergeManager[key].block_id_list.push_back(make_tuple(result.block_id,block_start_offset,block_size,true));

        // rowfilter =  "[Check Merge Buffer]\n";
        // rowfilter += "-------------------------------Merging Block Info----------------------------------\n";
        // rowfilter += "| work id: " + to_string(m_MergeManager[key].work_id)+ " | length: " + to_string(m_MergeManager[key].length) 
        //         + " | rows: " + to_string(m_MergeManager[key].rows) + " | \n";
        // for(int i=0; i<m_MergeManager[key].block_id_list.size(); i++){
        //     rowfilter += "block list[" + to_string(i) + "] : { " + to_string(get<0>(m_MergeManager[key].block_id_list[i])) + " : " 
        //     +  to_string(get<1>(m_MergeManager[key].block_id_list[i])) + " : " +  to_string(get<2>(m_MergeManager[key].block_id_list[i])) 
        //     + " : " +  to_string(get<3>(m_MergeManager[key].block_id_list[i])) + " }\n";
        // }
        // rowfilter += "------------------------------------------------------------------------------------\n";
        
        // strcpy(msg.msg,rowfilter.c_str());
        // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
        //     printf("msgsnd failed\n");
        //     exit(0);
        // }
        
        // ostringstream osss;
        // for(int i = 0; i < m_MergeManager[key].length; i++){
        //     osss << hex << (int)m_MergeManager[key].data[i];
        // }

        // strcpy(msg.msg,osss.str().c_str());
        // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
        //     printf("msgsnd failed\n");
        //     exit(0);
        // }

        // rowfilter = "------------------------------------------------------------------------------------\n";
        

        // strcpy(msg.msg,rowfilter.c_str());
        // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
        //     printf("msgsnd failed\n");
        //     exit(0);
        // }
    }

    m_MergeManager[key].block_count += 1;

    // cout << "[" << m_MergeManager[key].block_count << "/" << m_MergeManager[key].total_block_count << "] ";

    if(m_MergeManager[key].total_block_count == m_MergeManager[key].block_count){//블록 병합이 끝나면
        // rowfilter = "\n[Send Merged Block Data To Buffer Manager]";
        // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
        //     printf("msgsnd failed\n");
        //     exit(0);
        // }
        ReturnQueue.push_work(m_MergeManager[key]);
    }
    
}
