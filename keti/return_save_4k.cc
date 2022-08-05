#include "return.h"

void Return::ReturnResult(){

    while (1){
        MergeResult mergeResult = ReturnQueue.wait_and_pop();
                
        SendDataToBufferManager(mergeResult);
    }
}

void Return::SendDataToBufferManager(MergeResult &mergeResult){
    //cout << "\n[Send Data To Buffer Manager]" << endl;
    //mergeBlock을 json ?
    //printf("~~Send Data To BufferManager~~ # workid: %d, rows: %d, length: %d, offset_len: %ld, block_list_len: %ld\n",mergedBlock.work_id, mergedBlock.rows, mergedBlock.length, mergedBlock.row_offset.size(), mergedBlock.block_id_list.size());
    // key_t key1=54321;
    // int msqid;
    // message msg;
    // msg.msg_type=1;
    // if((msqid=msgget(key1,IPC_CREAT|0666))==-1){
    //     printf("msgget failed\n");
    //     exit(0);
    // }
    // string rowfilter =  "[Send Merged Block Data To Buffer Manager]\n";
    // rowfilter += "-------------------------------Merged Block Info----------------------------------\n";
    // rowfilter += "| work id: " + to_string(mergeResult.work_id)+ " | length: " + to_string(mergeResult.length) 
    //           + " | rows: " + to_string(mergeResult.rows) + " | \n";
    // for(int i=0; i<mergeResult.block_id_list.size(); i++){
    //     rowfilter += "block list[" + to_string(i) + "] : { " + to_string(get<0>(mergeResult.block_id_list[i])) + " : " 
    //     +  to_string(get<1>(mergeResult.block_id_list[i])) + " : " +  to_string(get<2>(mergeResult.block_id_list[i])) 
    //     + " : " +  to_string(get<3>(mergeResult.block_id_list[i])) + " }\n";
    // }
    // rowfilter += "------------------------------------------------------------------------------------\n";
    
    // cout << "---------------Merged Block Info-----------------" << endl;
    // cout << "| work id: " << mergeResult.work_id << " | length: " << mergeResult.length
    //      << " | rows: " << mergeResult.rows << " | " << "block list : ";
    // for(int i=0; i<mergeResult.block_id_list.size(); i++){
    //     cout << "{" << get<0>(mergeResult.block_id_list[i]) << " : " << get<3>(mergeResult.block_id_list[i]) + "},";
    // // }
    // cout << "mergeResult.length: " << mergeResult.length << endl;
    // cout << "\n----------------------------------------------\n";
    // for(int i = 0; i < mergeResult.length; i++){
    //     printf("%02X",(u_char)mergeResult.data[i]);
    // }
    // cout << "------------------------------------------------\n";

    // strcpy(msg.msg,rowfilter.c_str());
    // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
    //     printf("msgsnd failed\n");
    //     exit(0);
    // }
    
    // ostringstream osss;
    // for(int i = 0; i < mergeResult.length; i++){
    //     osss << hex << (int)mergeResult.data[i];
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

    StringBuffer block_buf;
    PrettyWriter<StringBuffer> writer(block_buf);

    writer.StartObject();
    writer.Key("Work ID");
    writer.Int(mergeResult.work_id);
    writer.Key("Block ID");
    writer.StartArray();

    for (int i = 0; i < mergeResult.block_id_list.size(); i ++){
        writer.StartArray();
        writer.Int(get<0>(mergeResult.block_id_list[i]));
        writer.Int(get<1>(mergeResult.block_id_list[i]));
        writer.Int(get<2>(mergeResult.block_id_list[i]));
        writer.Bool(get<3>(mergeResult.block_id_list[i]));
        writer.EndArray();
    }
    
    writer.EndArray();
    writer.Key("nrows");
    writer.Int(mergeResult.rows);
    writer.Key("Row Offset");
    writer.StartArray();
    for (int i = 0; i < mergeResult.row_offset.size(); i ++){
        writer.Int(mergeResult.row_offset[i]);
    }
    writer.EndArray();
    writer.Key("Length");
    writer.Int(mergeResult.length);

    writer.Key("CSD Name");
    writer.String(mergeResult.csd_name.c_str());

    writer.EndObject();

    string block_buf_ = block_buf.GetString();

    int sockfd;
    struct sockaddr_in serv_addr;
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("10.0.5.33");
    serv_addr.sin_port = htons(8888);

    connect(sockfd,(const sockaddr*)&serv_addr,sizeof(serv_addr));
	
	size_t len = strlen(block_buf_.c_str());
	send(sockfd, &len, sizeof(len), 0);
    send(sockfd, (char*)block_buf_.c_str(), len, 0);

    static char cBuffer[PACKET_SIZE];
    if (recv(sockfd, cBuffer, PACKET_SIZE, 0) == 0){
        cout << "client recv Error" << endl;
        return;
    };

    len = mergeResult.length;
    send(sockfd,&len,sizeof(len),0);
    send(sockfd, mergeResult.data, 4096, 0);
    // free(mergeResult.data);

    // printf("~MergeBlock~ # workid: %d, blockid: %d, rows: %d, length: %d, offset_len: %ld\n",result.work_id, result.block_id, result.rows, result.totallength, result.offset.size());
    // rowfilter = "[Json Send To Buffer Manager]\n";
    // rowfilter += block_buf_;
    
    // strcpy(msg.msg,rowfilter.c_str());
    // if(msgsnd(msqid,&msg,sizeof(msg.msg),0)==-1){
    //     printf("msgsnd failed\n");
    //     exit(0);
    // }
    
    close(sockfd);
}