#include "merge_manager.h"

void MergeManager::Merging(){
    while (1){
        Result result = MergeQueue.wait_and_pop();              
        MergeBlock(result);
    }
}

void MergeManager::MergeBlock(Result &result){
	    
    int row_len = 0;
    int row_num = 0;
    int key = result.work_id;

    //Key에 해당하는 블록버퍼가 없다면 생성
    if(m_MergeManager.find(key)==m_MergeManager.end()){
        MergeResult block(key, result.csd_name, result.total_block_count);
        m_MergeManager.insert(make_pair(key,block));
    }

    //data가 있을경우 수행
    if(result.row_count != 0){
        vector<int> temp_offset;
        temp_offset.assign( result.row_offset.begin(), result.row_offset.end() );
        temp_offset.push_back(result.length);

        if(result.filter_info.groupby_col.size() == 0){//groupBy X



        }else{//groupBy O

        }

        //새 40k 블록 넣기 전 확인
        row_len = temp_offset[1] - temp_offset[0];

        if(m_MergeManager[key].length + row_len > BUFF_SIZE){//row추가시 데이터 크기 넘으면
            ReturnQueue.push_work(m_MergeManager[key]);
            m_MergeManager[key].InitMergeResult();
        }

        //새 40k 블록 넣기
        for(int i=0; i<result.row_count; i++){
            row_len = temp_offset[i+1] - temp_offset[i];
            if(m_MergeManager[key].length + row_len > BUFF_SIZE){//row추가시 데이터 크기 넘으면
                ReturnQueue.push_work(m_MergeManager[key]);
                m_MergeManager[key].InitMergeResult();
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

    }

    m_MergeManager[key].current_block_count += result.result_block_count;
    m_MergeManager[key].result_block_count += result.result_block_count;

    if(m_MergeManager[key].total_block_count == m_MergeManager[key].current_block_count){//블록 병합이 끝나면
        ReturnQueue.push_work(m_MergeManager[key]);
        m_MergeManager[key].InitMergeResult();
        cout << "WORK [" << key << "] DONE" << endl;
    }
    
}
