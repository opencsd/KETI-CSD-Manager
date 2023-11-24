#include "return.h"

void Return::ReturnResult(){
    // cout << "<-----------  Return Layer Running...  ----------->\n";
    while (1){
        MergeResult mergeResult = ReturnQueue.wait_and_pop();
                
        SendDataToBufferManager(mergeResult);
    }
}

void Return::SendDataToBufferManager(MergeResult &mergeResult){
    float temp_size = float(mergeResult.length) / float(1024);
    printf("[CSD Return Interface] Send Data to Buffer Manager (Return Buffer Size : %.1fK)\n",temp_size);

    
    // // 테스트 출력
    // cout << "---------------Merged Block Info-----------------" << endl;
    // cout << "| work id: " << mergeResult.work_id << " | length: " << mergeResult.length
    //      << " | row count: " << mergeResult.row_count << endl;
    // cout << "result.length: " << mergeResult.length << endl;
    // cout << "result.row_offset: ";
    // for(int i = 0; i < mergeResult.row_offset.size(); i++){
    //     printf("%d ",mergeResult.row_offset[i]);
    // }
    // cout << "\n----------------------------------------------\n";
    // for(int i = 0; i < mergeResult.length; i++){
    //     printf("%02X",(u_char)mergeResult.data[i]);
    // }
    // cout << "\n------------------------------------------------\n";

    StringBuffer block_buf;
    PrettyWriter<StringBuffer> writer(block_buf);

    writer.StartObject();

    writer.Key("queryID");
    writer.Int(mergeResult.query_id);

    writer.Key("workID");
    writer.Int(mergeResult.work_id);

    writer.Key("rowCount");
    writer.Int(mergeResult.row_count);

    writer.Key("rowOffset");
    writer.StartArray();
    for (int i = 0; i < mergeResult.row_offset.size(); i ++){
        writer.Int(mergeResult.row_offset[i]);
    }
    writer.EndArray();

    writer.Key("length");
    writer.Int(mergeResult.length);

    writer.Key("csdName");
    writer.String(mergeResult.csd_name.c_str());

    // writer.Key("Last valid block id");
    // writer.Int(mergeResult.last_valid_block_id);

    writer.Key("resultBlockCount");
    writer.Int(mergeResult.result_block_count);

    writer.EndObject();

    return;//안보내기

    string block_buf_ = block_buf.GetString();

    int sockfd;
    struct sockaddr_in serv_addr;
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(BUFF_M_IP);
    serv_addr.sin_port = htons(BUFF_M_PORT);

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
    send(sockfd, mergeResult.data, BUFF_SIZE, 0);
    
    close(sockfd);
}