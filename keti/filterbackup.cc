#include "filter.h"
extern queue<ScanResult> FilterQueue;
void Filter::Filtering()
{
    cout << "\n<-----------  Filter Layer Running...  ----------->\n"
         << endl;

    while (1)
    {
        if (!FilterQueue.empty())
        {
            ScanResult &scanResult = FilterQueue.front();
            BlockFilter(scanResult);

            FilterQueue.pop();
        }
    }
}

int Filter::BlockFilter(ScanResult scanResult)
{
    char *rowbuf = scanResult.scan_buf;

    unordered_map<string, int> startptr;
    unordered_map<string, int> lengthRaw;
    unordered_map<string, int> typedata;
    unordered_map<string, int> newstartptr;
    unordered_map<string, int> newlengthraw;

    FilterResult filterresult(scanResult.work_id, scanResult.block_id, 0, 0, 0, NULL);

    int ColNum = scanResult.table_col.Size(); //컬럼 넘버로 컬럼의 수를 의미(스니펫을 통해 받은 컬럼의 수)
    int RowNum = scanResult.rows;             //로우 넘버로 로우의 수를 의미(스캔에서 받은 로우의 수)
    int startoff[ColNum];
    int offlen[ColNum];
    int datatype[ColNum];
    for (int i = 0; i < ColNum; i++)
    {
        startoff[i] = scanResult.table_offset[i].GetInt();
        offlen[i] = scanResult.table_offlen[i].GetInt();
        datatype[i] = scanResult.table_datatype[i].GetInt();
    }
    std::string ColName[ColNum]; //스니펫을 통해 받은 각 컬럼의 이름이 저장되는 배열
    bool CV, TmpV;               // CV는 현재 연산의 결과, TmpV는 이전 연산 까지의 결과
    int lvtype;
    int rvtype; // little big decimal, int float string
    for (int i = 0; i < ColNum; i++)
    {
        ColName[i] = scanResult.table_col[i].GetInt(); //현제 테스트를 위해 위에 사전 정의된 데이터를 넣어줌
    }
    // for (int i = 0; i < ClausesNum; i++)
    // {
    //     WhereClauses[i] = where11[i]; //현제 테스트를 위해 위에 사전 정의된 데이터를 넣어줌
    //     // cout << "insert whereclauses : " << WhereClauses[i] << endl;
    // }
    bool Passed;            // and조건 이전이 f일 경우 연산을 생략하는 함수
    bool isSaved, canSaved; // or을 통해 저장이 되었는지, and 또는 or에서 저장이 가능한지 를 나타내는 변수
    bool isnot;             //이전 not operator를 만낫는지에 대한 변수
    bool isvarchar = 0;     // varchar 형을 포함한 컬럼인지에 대한 변수

    isvarchar = isvarc(datatype, ColNum);
    makedefaultmap(ColName, startoff, offlen, datatype, ColNum, startptr, lengthRaw, typedata);

    // map<string,string> OneRow;
    // cout << "!" << endl;
    Value &filterarray = scanResult.table_filter;
    // cout << "!" << endl;

    int iter = 0; //각 row의 시작점
    for (int i = 0; i < RowNum; i++)
    {
        makenewmap(isvarchar, ColNum, newstartptr, newlengthraw, datatype, lengthRaw, ColName, iter, startoff, offlen, rowbuf);
        TmpV = true;
        Passed = false;
        isSaved = false;
        canSaved = false;
        isnot = false;

        for (int j = 0; j < filterarray.Size(); j++)
        {
            // cout << filterarray[j]["OPERATOR"].GetInt() << endl;
            // cout << j << endl;
            // cout << filterarray[j]["OPERATOR"].GetInt() << endl;
            switch (filterarray[j]["OPERATOR"].GetInt())
            {
            case GE:
                if (Passed)
                {
                    cout << "*Row Filtered*" << endl;
                    break;
                }
                else
                {
                    if (filterarray[j]["LV"].GetType() == 5)
                    {                                                                                                            // 6은 스트링 --> 스트링이다는 컬럼이름이거나 char이거나 decimal이다
                        if (typedata[filterarray[j]["LV"].GetString()] == 3 || typedata[filterarray[j]["LV"].GetString()] == 14) //리틀에디안
                        {
                            // cout << typedata[filterarray[j]["LV"].GetString()] << endl;
                            int LV = typeLittle(typedata, filterarray[j]["LV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                            int RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                RV = typeLittle(typedata, filterarray[j]["RV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                            }
                            else
                            {
                                RV = filterarray[j]["RV"].GetInt();
                            }
                            // cout << LV << " " << RV << endl;
                            compareGE(LV, RV, CV, TmpV, canSaved, isnot);
                        }
                        else if (typedata[filterarray[j]["LV"].GetString()] == 254 || typedata[filterarray[j]["LV"].GetString()] == 15) //빅에디안
                        {
                            string LV = typeBig(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                            string RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                if (typedata[filterarray[j]["RV"].GetString()] == 254 || typedata[filterarray[j]["RV"].GetString()] == 15)
                                {
                                    RV = typeBig(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                                }
                                else
                                {
                                    RV = filterarray[j]["RV"].GetString();
                                    RV = RV.substr(1);
                                }
                            }
                            else
                            { // string과 int의 비교 시
                            }

                            compareGE(LV, RV, CV, TmpV, canSaved, isnot);
                            // cout << "type big" << endl;
                        }
                        else if (typedata[filterarray[j]["LV"].GetString()] == 246) //예외 Decimal일때
                        {
                            string LV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                            string RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                if (typedata[filterarray[j]["RV"].GetString()] == 246)
                                {
                                    RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                                }
                                else
                                {
                                    RV = filterarray[j]["RV"].GetString();
                                    RV = RV.substr(1);
                                }
                            }
                            else
                            { //여기가 int를 데시멀로 바꾸는 부분
                                int tmpint;
                                tmpint = filterarray[j]["RV"].GetInt();
                                RV = ItoDec(tmpint);
                            }
                            compareGE(LV, RV, CV, TmpV, canSaved, isnot);
                            // cout << "type decimal" << endl;
                        }
                        else
                        {
                            string LV = filterarray[j]["LV"].GetString();
                            LV = LV.substr(1);
                            string RV;
                            if (typedata[filterarray[j]["RV"].GetString()] == 254 || typedata[filterarray[j]["RV"].GetString()] == 15)
                            {
                                RV = typeBig(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            else if (typedata[filterarray[j]["RV"].GetString()] == 246)
                            {
                                RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            compareGE(LV, RV, CV, TmpV, canSaved, isnot);
                        }
                    }
                    else
                    { // lv는 인트타입의 상수
                        int LV = filterarray[j]["LV"].GetInt();
                        int RV;
                        RV = typeLittle(typedata, filterarray[j]["RV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                        compareGE(LV, RV, CV, TmpV, canSaved, isnot);
                    }
                }
                /* code */
                break;
            case LE:
                if (Passed)
                {
                    cout << "*Row Filtered*" << endl;
                    break;
                }
                else
                {
                    if (filterarray[j]["LV"].GetType() == 5)
                    {                                                                                                            // 6은 스트링 --> 스트링이다는 컬럼이름이거나 char이거나 decimal이다
                        if (typedata[filterarray[j]["LV"].GetString()] == 3 || typedata[filterarray[j]["LV"].GetString()] == 14) //리틀에디안
                        {
                            int LV = typeLittle(typedata, filterarray[j]["LV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                            int RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                RV = typeLittle(typedata, filterarray[j]["RV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                            }
                            else
                            {
                                RV = filterarray[j]["RV"].GetInt();
                            }
                            compareLE(LV, RV, CV, TmpV, canSaved, isnot);
                        }
                        else if (typedata[filterarray[j]["LV"].GetString()] == 254 || typedata[filterarray[j]["LV"].GetString()] == 15) //빅에디안
                        {
                            string LV = typeBig(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                            string RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                RV = typeBig(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            else
                            {
                                RV = filterarray[j]["RV"].GetString();
                                RV = RV.substr(1);
                            }
                            compareLE(LV, RV, CV, TmpV, canSaved, isnot);
                            // cout << "type big" << endl;
                        }
                        else if (typedata[filterarray[j]["LV"].GetString()] == 246) //예외 Decimal일때
                        {
                            string LV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                            string RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                if (typedata[filterarray[j]["RV"].GetString()] == 246)
                                {
                                    RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                                }
                                else
                                {
                                    RV = filterarray[j]["RV"].GetString();
                                    RV = RV.substr(1);
                                }
                            }
                            else
                            { //여기가 int를 데시멀로 바꾸는 부분
                                int tmpint;
                                tmpint = filterarray[j]["RV"].GetInt();
                                RV = ItoDec(tmpint);
                            }
                            compareLE(LV, RV, CV, TmpV, canSaved, isnot);
                            // cout << "type decimal" << endl;
                        }
                        else
                        {
                            string LV = filterarray[j]["LV"].GetString();
                            LV = LV.substr(1);
                            string RV;
                            if (typedata[filterarray[j]["RV"].GetString()] == 254 || typedata[filterarray[j]["RV"].GetString()] == 15)
                            {
                                RV = typeBig(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            else if (typedata[filterarray[j]["RV"].GetString()] == 246)
                            {
                                RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            compareLE(LV, RV, CV, TmpV, canSaved, isnot);
                        }
                    }
                    else
                    { // lv는 인트타입의 상수
                        int LV = filterarray[j]["LV"].GetInt();
                        int RV;
                        RV = typeLittle(typedata, filterarray[j]["RV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                        compareLE(LV, RV, CV, TmpV, canSaved, isnot);
                    }
                }
                /* code */
                break;
            case GT:
                if (Passed)
                {
                    cout << "*Row Filtered*" << endl;
                    break;
                }
                else
                {
                    if (filterarray[j]["LV"].GetType() == 5)
                    {                                                                                                            // 6은 스트링 --> 스트링이다는 컬럼이름이거나 char이거나 decimal이다
                        if (typedata[filterarray[j]["LV"].GetString()] == 3 || typedata[filterarray[j]["LV"].GetString()] == 14) //리틀에디안
                        {
                            int LV = typeLittle(typedata, filterarray[j]["LV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                            int RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                RV = typeLittle(typedata, filterarray[j]["RV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                            }
                            else
                            {
                                RV = filterarray[j]["RV"].GetInt();
                            }
                            compareGT(LV, RV, CV, TmpV, canSaved, isnot);
                        }
                        else if (typedata[filterarray[j]["LV"].GetString()] == 254 || typedata[filterarray[j]["LV"].GetString()] == 15) //빅에디안
                        {
                            string LV = typeBig(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                            string RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                RV = typeBig(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            else
                            {
                                RV = filterarray[j]["RV"].GetString();
                                RV = RV.substr(1);
                            }
                            compareGT(LV, RV, CV, TmpV, canSaved, isnot);
                            // cout << "type big" << endl;
                        }
                        else if (typedata[filterarray[j]["LV"].GetString()] == 246) //예외 Decimal일때
                        {
                            string LV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                            string RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                if (typedata[filterarray[j]["RV"].GetString()] == 246)
                                {
                                    RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                                }
                                else
                                {
                                    RV = filterarray[j]["RV"].GetString();
                                    RV = RV.substr(1);
                                }
                            }
                            else
                            { //여기가 int를 데시멀로 바꾸는 부분
                                int tmpint;
                                tmpint = filterarray[j]["RV"].GetInt();
                                RV = ItoDec(tmpint);
                            }
                            compareGT(LV, RV, CV, TmpV, canSaved, isnot);
                            // cout << "type decimal" << endl;
                        }
                        else
                        {
                            string LV = filterarray[j]["LV"].GetString();
                            LV = LV.substr(1);
                            string RV;
                            if (typedata[filterarray[j]["RV"].GetString()] == 254 || typedata[filterarray[j]["RV"].GetString()] == 15)
                            {
                                RV = typeBig(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            else if (typedata[filterarray[j]["RV"].GetString()] == 246)
                            {
                                RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            compareGT(LV, RV, CV, TmpV, canSaved, isnot);
                        }
                    }
                    else
                    { // lv는 인트타입의 상수
                        int LV = filterarray[j]["LV"].GetInt();
                        int RV;
                        RV = typeLittle(typedata, filterarray[j]["RV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                        compareGT(LV, RV, CV, TmpV, canSaved, isnot);
                    }
                }
                /* code */
                break;
            case LT:
                if (Passed)
                {
                    cout << "*Row Filtered*" << endl;
                    break;
                }
                else
                {
                    if (filterarray[j]["LV"].GetType() == 5)
                    {                                                                                                            // 6은 스트링 --> 스트링이다는 컬럼이름이거나 char이거나 decimal이다
                        if (typedata[filterarray[j]["LV"].GetString()] == 3 || typedata[filterarray[j]["LV"].GetString()] == 14) //리틀에디안
                        {
                            // cout << typedata[filterarray[j]["LV"].GetString()] << endl;
                            int LV = typeLittle(typedata, filterarray[j]["LV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                            int RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                RV = typeLittle(typedata, filterarray[j]["RV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                            }
                            else
                            {
                                RV = filterarray[j]["RV"].GetInt();
                            }
                            // cout << LV << " " << RV << endl;
                            compareLT(LV, RV, CV, TmpV, canSaved, isnot);
                        }
                        else if (typedata[filterarray[j]["LV"].GetString()] == 254 || typedata[filterarray[j]["LV"].GetString()] == 15) //빅에디안
                        {
                            string LV = typeBig(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                            string RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                RV = typeBig(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            else
                            {
                                RV = filterarray[j]["RV"].GetString();
                                RV = RV.substr(1);
                            }
                            compareLT(LV, RV, CV, TmpV, canSaved, isnot);
                            // cout << "type big" << endl;
                        }
                        else if (typedata[filterarray[j]["LV"].GetString()] == 246) //예외 Decimal일때
                        {
                            string LV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                            string RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                if (typedata[filterarray[j]["RV"].GetString()] == 246)
                                {
                                    RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                                }
                                else
                                {
                                    RV = filterarray[j]["RV"].GetString();
                                    RV = RV.substr(1);
                                }
                            }
                            else
                            { //여기가 int를 데시멀로 바꾸는 부분
                                int tmpint;
                                tmpint = filterarray[j]["RV"].GetInt();
                                RV = ItoDec(tmpint);
                            }
                            compareLT(LV, RV, CV, TmpV, canSaved, isnot);
                            // cout << "type decimal" << endl;
                        }
                        else
                        {
                            string LV = filterarray[j]["LV"].GetString();
                            LV = LV.substr(1);
                            string RV;
                            if (typedata[filterarray[j]["RV"].GetString()] == 254 || typedata[filterarray[j]["RV"].GetString()] == 15)
                            {
                                RV = typeBig(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            else if (typedata[filterarray[j]["RV"].GetString()] == 246)
                            {
                                RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            compareLT(LV, RV, CV, TmpV, canSaved, isnot);
                        }
                    }
                    else
                    { // lv는 인트타입의 상수
                        int LV = filterarray[j]["LV"].GetInt();
                        int RV;
                        RV = typeLittle(typedata, filterarray[j]["RV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                        compareLT(LV, RV, CV, TmpV, canSaved, isnot);
                    }
                }
                /* code */
                break;
            case ET:
                if (Passed)
                {
                    cout << "*Row Filtered*" << endl;
                    break;
                }
                else
                {
                    if (filterarray[j]["LV"].GetType() == 5)
                    {                                                                                                            // 6은 스트링 --> 스트링이다는 컬럼이름이거나 char이거나 decimal이다
                        if (typedata[filterarray[j]["LV"].GetString()] == 3 || typedata[filterarray[j]["LV"].GetString()] == 14) //리틀에디안
                        {
                            int LV = typeLittle(typedata, filterarray[j]["LV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                            int RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                RV = typeLittle(typedata, filterarray[j]["RV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                            }
                            else
                            {
                                RV = filterarray[j]["RV"].GetInt();
                            }
                            compareET(LV, RV, CV, TmpV, canSaved, isnot);
                        }
                        else if (typedata[filterarray[j]["LV"].GetString()] == 254 || typedata[filterarray[j]["LV"].GetString()] == 15) //빅에디안
                        {
                            string LV = typeBig(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                            string RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                RV = typeBig(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            else
                            {
                                RV = filterarray[j]["RV"].GetString();
                                RV = RV.substr(1);
                            }
                            compareET(LV, RV, CV, TmpV, canSaved, isnot);
                            // cout << "type big" << endl;
                        }
                        else if (typedata[filterarray[j]["LV"].GetString()] == 246) //예외 Decimal일때
                        {
                            string LV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                            string RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                if (typedata[filterarray[j]["RV"].GetString()] == 246)
                                {
                                    RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                                }
                                else
                                {
                                    RV = filterarray[j]["RV"].GetString();
                                    RV = RV.substr(1);
                                }
                            }
                            else
                            { //여기가 int를 데시멀로 바꾸는 부분
                                int tmpint;
                                tmpint = filterarray[j]["RV"].GetInt();
                                RV = ItoDec(tmpint);
                            }
                            compareET(LV, RV, CV, TmpV, canSaved, isnot);
                            // cout << "type decimal" << endl;
                        }
                        else
                        {
                            string LV = filterarray[j]["LV"].GetString();
                            LV = LV.substr(1);
                            string RV;
                            if (typedata[filterarray[j]["RV"].GetString()] == 254 || typedata[filterarray[j]["RV"].GetString()] == 15)
                            {
                                RV = typeBig(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            else if (typedata[filterarray[j]["RV"].GetString()] == 246)
                            {
                                RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            compareET(LV, RV, CV, TmpV, canSaved, isnot);
                        }
                    }
                    else
                    { // lv는 인트타입의 상수
                        int LV = filterarray[j]["LV"].GetInt();
                        int RV;
                        RV = typeLittle(typedata, filterarray[j]["RV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                        compareET(LV, RV, CV, TmpV, canSaved, isnot);
                    }
                }
                /* code */
                break;
            case NE:
                if (Passed)
                {
                    cout << "*Row Filtered*" << endl;
                    break;
                }
                else
                {
                    if (filterarray[j]["LV"].GetType() == 5)
                    {                                                                                                            // 6은 스트링 --> 스트링이다는 컬럼이름이거나 char이거나 decimal이다
                        if (typedata[filterarray[j]["LV"].GetString()] == 3 || typedata[filterarray[j]["LV"].GetString()] == 14) //리틀에디안
                        {
                            int LV = typeLittle(typedata, filterarray[j]["LV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                            int RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                RV = typeLittle(typedata, filterarray[j]["RV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                            }
                            else
                            {
                                RV = filterarray[j]["RV"].GetInt();
                            }
                            compareNE(LV, RV, CV, TmpV, canSaved, isnot);
                        }
                        else if (typedata[filterarray[j]["LV"].GetString()] == 254 || typedata[filterarray[j]["LV"].GetString()] == 15) //빅에디안
                        {
                            string LV = typeBig(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                            string RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                RV = typeBig(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            else
                            {
                                RV = filterarray[j]["RV"].GetString();
                                RV = RV.substr(1);
                            }
                            compareNE(LV, RV, CV, TmpV, canSaved, isnot);
                            // cout << "type big" << endl;
                        }
                        else if (typedata[filterarray[j]["LV"].GetString()] == 246) //예외 Decimal일때
                        {
                            string LV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                            string RV;
                            if (filterarray[j]["RV"].GetType() == 5)
                            {
                                if (typedata[filterarray[j]["RV"].GetString()] == 246)
                                {
                                    RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                                }
                                else
                                {
                                    RV = filterarray[j]["RV"].GetString();
                                    RV = RV.substr(1);
                                }
                            }
                            else
                            { //여기가 int를 데시멀로 바꾸는 부분
                                int tmpint;
                                tmpint = filterarray[j]["RV"].GetInt();
                                RV = ItoDec(tmpint);
                            }
                            compareNE(LV, RV, CV, TmpV, canSaved, isnot);
                            // cout << "type decimal" << endl;
                        }
                        else
                        {
                            string LV = filterarray[j]["LV"].GetString();
                            LV = LV.substr(1);
                            string RV;
                            if (typedata[filterarray[j]["RV"].GetString()] == 254 || typedata[filterarray[j]["RV"].GetString()] == 15)
                            {
                                RV = typeBig(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            else if (typedata[filterarray[j]["RV"].GetString()] == 246)
                            {
                                RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            }
                            compareNE(LV, RV, CV, TmpV, canSaved, isnot);
                        }
                    }
                    else
                    { // lv는 인트타입의 상수
                        int LV = filterarray[j]["LV"].GetInt();
                        int RV;
                        RV = typeLittle(typedata, filterarray[j]["RV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                        compareNE(LV, RV, CV, TmpV, canSaved, isnot);
                    }
                }
                /* code */
                break;
            case LIKE:
                if (Passed)
                {
                    cout << "*Row Filtered*" << endl;
                    // cout << "isnot print :" << isnot << " value : " << LV << endl;
                }
                else
                {
                    string RV;
                    string LV;
                    if (typedata[filterarray[j]["LV"].GetString()] == 3 || typedata[filterarray[j]["LV"].GetString()] == 14) //리틀에디안
                    {
                        int tmplv;
                        tmplv = typeLittle(typedata, filterarray[j]["LV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                        LV = to_string(tmplv);
                        // cout << "type little" << endl;
                        //나중 다른 데이트 처리를 위한 구분
                    }

                    else if (typedata[filterarray[j]["LV"].GetString()] == 254 || typedata[filterarray[j]["LV"].GetString()] == 15) //빅에디안
                    {
                        LV = typeBig(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                        // cout << "type big" << endl;
                    }
                    else if (typedata[filterarray[j]["LV"].GetString()] == 246) //예외 Decimal일때
                    {
                        LV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                        // cout << "type decimal" << endl;
                    }
                    else
                    {
                        LV = filterarray[j]["LV"].GetString();
                        LV = LV.substr(1);
                    }
                    if (typedata[filterarray[j]["RV"].GetString()] == 3 || typedata[filterarray[j]["RV"].GetString()] == 14) //리틀에디안
                    {
                        int tmprv;
                        tmprv = typeLittle(typedata, filterarray[j]["RV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                        RV = to_string(tmprv);
                        // cout << "type little" << endl;
                        //나중 다른 데이트 처리를 위한 구분
                    }

                    else if (typedata[filterarray[j]["RV"].GetString()] == 254 || typedata[filterarray[j]["RV"].GetString()] == 15) //빅에디안
                    {
                        RV = typeBig(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                        // cout << "type big" << endl;
                    }
                    else if (typedata[filterarray[j]["RV"].GetString()] == 246) //예외 Decimal일때
                    {
                        RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                        // cout << "type decimal" << endl;
                    }
                    else
                    {
                        RV = filterarray[j]["RV"].GetString();
                        RV = RV.substr(1);
                    }
                    CV = LikeSubString_v2(LV, RV);
                    // cout << CV << endl;
                }
                // cout << "isnot print :" << isnot << " value : " << LV << endl;
                // cout << isnot << endl;
                if (isnot)
                {
                    if (CV)
                    {
                        CV = false;
                        canSaved = false;
                    }
                    else
                    {
                        CV = true;
                        canSaved = true;
                    }
                }
                /* code */
                break;
            case BETWEEN:
                if (Passed)
                {
                    cout << "*Row Filtered*" << endl;
                }
                else
                {
                    // cout << typedata[filterarray[j]["LV"].GetString()] << endl;
                    // cout << filterarray[j]["EXTRA"][0].GetString() << endl;
                    //  cout << filterarray[j]["LV"].GetString() << endl;
                    // cout << j << endl;
                    //  cout << filterarray[j]["LV"].GetType() << endl;
                    if (filterarray[j]["LV"].GetType() == 5)
                    {
                        // cout << "type : 5" << endl;
                        // string filtersring = filterarray[j]["LV"].GetString();
                        // cout << typedata[filtersring] << endl;
                        // cout << typedata[filterarray[j]["LV"].GetString()] << endl;                                                                                                  // 6은 스트링 --> 스트링이다는 컬럼이름이거나 char이거나 decimal이다
                        if (typedata[filterarray[j]["LV"].GetString()] == 3 || typedata[filterarray[j]["LV"].GetString()] == 14) //리틀에디안
                        {
                            int LV = typeLittle(typedata, filterarray[j]["LV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                            int RV;
                            int RV1;
                            for (int k = 0; k < filterarray[j]["EXTRA"].Size(); k++)
                            {
                                if (filterarray[j]["EXTRA"][k].GetType() == 5)
                                { //컬럼명 또는 스트링이다. --> 스트링을 int로 변경, 만약 변경 불가한 문자의 경우 ex. 'asd' 예외처리해서 걍 f로 반환
                                    if (typedata[filterarray[j]["EXTRA"][k].GetString()] == 3 || typedata[filterarray[j]["EXTRA"][k].GetString()] == 14)
                                    {
                                        if (k == 0)
                                        {
                                            RV = typeLittle(typedata, filterarray[j]["EXTRA"][k].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                                        }
                                        else
                                        {
                                            RV1 = typeLittle(typedata, filterarray[j]["EXTRA"][k].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                                        }
                                    }
                                    else
                                    { //스트링이다 --> 변환 가능한가
                                        if (k == 0)
                                        {
                                            try
                                            {
                                                RV = stoi(filterarray[j]["EXTRA"][k].GetString());
                                            }
                                            catch (...)
                                            {
                                                CV = true; //수정 필요
                                                break;
                                            }
                                        }
                                        else
                                        {
                                            try
                                            {
                                                RV1 = stoi(filterarray[j]["EXTRA"][k].GetString());
                                            }
                                            catch (...)
                                            {
                                                CV = true;
                                                break;
                                            }
                                        }
                                    }
                                }
                                else if (filterarray[j]["EXTRA"][k].GetType() == 6) // int,float 타입
                                {                                                   // int, float, double
                                    if (filterarray[j]["EXTRA"][k].IsInt())
                                    {
                                        if (k == 0)
                                        {
                                            RV = filterarray[j]["EXTRA"][k].GetInt();
                                        }
                                        else
                                        {
                                            RV1 = filterarray[j]["EXTRA"][k].GetInt();
                                        }
                                    }
                                    else
                                    { // float일 경우는 없음 --> 스트링으로 들어오기 때문에
                                        float RV = filterarray[j]["EXTRA"][k].GetFloat();
                                    }
                                }
                            }
                            CV = BetweenOperator(LV, RV, RV1);
                        }
                        else if (typedata[filterarray[j]["LV"].GetString()] == 254 || typedata[filterarray[j]["LV"].GetString()] == 15) //빅에디안
                        {

                            string LV = typeBig(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                            string RV;
                            string RV1;
                            for (int k = 0; k < filterarray[j]["EXTRA"].Size(); k++)
                            {
                                if (filterarray[j]["EXTRA"][k].GetType() == 5)
                                { //컬럼명 또는 스트링이다. --> 스트링이다 == float가 decimal로 800000000으로 들어온다
                                    if (typedata[filterarray[j]["EXTRA"][k].GetString()] == 3 || typedata[filterarray[j]["EXTRA"][k].GetString()] == 14)
                                    {
                                        if (k == 0)
                                        {
                                            RV = typeBig(newlengthraw, newstartptr, filterarray[j]["EXTRA"][k].GetString(), rowbuf, lvtype);
                                        }
                                        else
                                        {
                                            RV1 = typeBig(newlengthraw, newstartptr, filterarray[j]["EXTRA"][k].GetString(), rowbuf, lvtype);
                                        }
                                    }
                                    else
                                    { //스트링이다 --> 변환 가능한가
                                        if (k == 0)
                                        {
                                            try
                                            {
                                                RV = filterarray[j]["EXTRA"][k].GetString();
                                                RV = RV.substr(1);
                                            }
                                            catch (...)
                                            {
                                                CV = true; //수정 필요
                                                break;
                                            }
                                        }
                                        else
                                        {
                                            try
                                            {
                                                RV1 = filterarray[j]["EXTRA"][k].GetString();
                                                RV1 = RV.substr(1);
                                            }
                                            catch (...)
                                            {
                                                CV = true;
                                                break;
                                            }
                                        }
                                    }
                                }
                                else if (filterarray[j]["EXTRA"][k].GetType() == 6) // int,float 타입 이 부분도 수정필요 string과 int의 비교
                                {                                                   // int, float, double
                                    int tmpint;
                                    if (filterarray[j]["EXTRA"][k].IsInt())
                                    {
                                        if (k == 0)
                                        {
                                            tmpint = filterarray[j]["EXTRA"][k].GetInt();
                                            RV = to_string(tmpint);
                                        }
                                        else
                                        {
                                            tmpint = filterarray[j]["EXTRA"][k].GetInt();
                                            RV1 = to_string(tmpint);
                                        }
                                    }
                                    else
                                    { // float일 경우는 없음 --> 스트링으로 들어오기 때문에
                                      // float RV = filterarray[j]["EXTRA"][k].GetFloat();
                                    }
                                }
                            }
                            CV = BetweenOperator(LV, RV, RV1);
                        }
                        else if (typedata[filterarray[j]["LV"].GetString()] == 246) //예외 Decimal일때
                        {
                            // cout << "type : 246" << endl;
                            // cout << typedata[filterarray[j]["LV"].GetString()] << endl;
                            // cout << "j : " << j << endl;
                            // cout << "246" << j << endl;
                            // cout << filtersring << endl;
                            string LV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                            string RV;
                            string RV1;
                            // cout << LV << endl;
                            for (int k = 0; k < filterarray[j]["EXTRA"].Size(); k++)
                            {
                                // cout << filterarray[j]["EXTRA"][k].GetType() << endl;
                                if (filterarray[j]["EXTRA"][k].GetType() == 5)
                                { //컬럼명 또는 스트링이다. --> 스트링이다 == float가 decimal로 800000000으로 들어온다
                                    if (typedata[filterarray[j]["EXTRA"][k].GetString()] == 3 || typedata[filterarray[j]["EXTRA"][k].GetString()] == 14)
                                    {
                                        if (k == 0)
                                        {
                                            RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["EXTRA"][k].GetString(), rowbuf, lvtype);
                                        }
                                        else
                                        {
                                            RV1 = typeDecimal(newlengthraw, newstartptr, filterarray[j]["EXTRA"][k].GetString(), rowbuf, lvtype);
                                        }
                                    }
                                    else
                                    { //스트링이다 --> 변환 가능한가
                                        if (k == 0)
                                        {
                                            try
                                            {
                                                RV = filterarray[j]["EXTRA"][k].GetString();
                                                RV = RV.substr(1);
                                            }
                                            catch (...)
                                            {
                                                CV = true; //수정 필요
                                                break;
                                            }
                                        }
                                        else
                                        {
                                            try
                                            {
                                                RV1 = filterarray[j]["EXTRA"][k].GetString();
                                                RV1 = RV1.substr(1);
                                            }
                                            catch (...)
                                            {
                                                CV = true;
                                                break;
                                            }
                                        }
                                    }
                                    // cout << "LV : " << LV << "RV : " << RV << "RV1 : " << RV1 << endl;
                                }
                                else if (filterarray[j]["EXTRA"][k].GetType() == 6) // int,float 타입 이 부분도 수정필요 string과 int의 비교
                                {                                                   // int, float, double
                                    int tmpint;
                                    if (filterarray[j]["EXTRA"][k].IsInt())
                                    {
                                        if (k == 0)
                                        {
                                            tmpint = filterarray[j]["EXTRA"][k].GetInt();
                                            RV = to_string(tmpint);
                                        }
                                        else
                                        {
                                            tmpint = filterarray[j]["EXTRA"][k].GetInt();
                                            RV1 = to_string(tmpint);
                                        }
                                    }
                                    else
                                    { // float일 경우는 없음 --> 스트링으로 들어오기 때문에
                                      // float RV = filterarray[j]["EXTRA"][k].GetFloat();
                                    }
                                }
                            }
                            CV = BetweenOperator(LV, RV, RV1);
                        }
                        else
                        { // lv가 데시멀일때
                            // cout << filterarray[j]["LV"].GetString() << endl;
                            string LV = filterarray[j]["LV"].GetString();
                            LV = LV.substr(1);
                            // string RV;
                            // if (typedata[filterarray[j]["RV"].GetString()] == 254 || typedata[filterarray[j]["RV"].GetString()] == 15)
                            // {
                            //     RV = typeBig(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            // }
                            // else if (typedata[filterarray[j]["RV"].GetString()] == 246)
                            // {
                            //     RV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["RV"].GetString(), rowbuf, lvtype);
                            // }
                            // compareNE(LV, RV, CV, TmpV, canSaved, isnot);
                        }
                    }
                    else
                    { // lv는 인트타입의 상수
                        int LV = filterarray[j]["LV"].GetInt();
                        // int RV;
                        // RV = typeLittle(typedata, filterarray[j]["RV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                        // compareNE(LV, RV, CV, TmpV, canSaved, isnot);
                    }
                }
                if (isnot)
                {
                    if (CV)
                    {
                        CV = false;
                        canSaved = false;
                    }
                    else
                    {
                        CV = true;
                        canSaved = true;
                    }
                }
                /* code */
                break;
            case IN: //고민이 좀 필요한 부분 만약 데이터타입이 다 맞춰서 들어온다면?
                /* code */
                // if (Passed)
                // {
                //     cout << "*Row Filtered*" << endl;
                //     break;
                // }
                // else
                // {
                //     if (filterarray[j]["LV"].GetType() == 5)
                //     {                                                                                                            // 6은 스트링 --> 스트링이다는 컬럼이름이거나 char이거나 decimal이다
                //         if (typedata[filterarray[j]["LV"].GetString()] == 3 || typedata[filterarray[j]["LV"].GetString()] == 14) //리틀에디안
                //         {
                //             int LV = typeLittle(typedata, filterarray[j]["LV"].GetString(), newlengthraw, newstartptr, rowbuf, lvtype);
                //             Value Extra = filterarray[j]["EXTRA"].GetArray();
                //             CV = InOperator(LV, Extra);
                //         }
                //         else if (typedata[filterarray[j]["LV"].GetString()] == 254 || typedata[filterarray[j]["LV"].GetString()] == 15) //빅에디안
                //         {
                //             string LV = typeBig(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                //             Value Extra = filterarray[j]["EXTRA"].GetArray();
                //             // Extra = filterarray[j]["EXTRA"].GetArray();
                //             CV = InOperator(LV, Extra);
                //             // cout << "type big" << endl;
                //         }
                //         else if (typedata[filterarray[j]["LV"].GetString()] == 246) //예외 Decimal일때
                //         {
                //             string LV = typeDecimal(newlengthraw, newstartptr, filterarray[j]["LV"].GetString(), rowbuf, lvtype);
                //             Value Extra;
                //             Extra = filterarray[j]["EXTRA"].GetArray();
                //             CV = InOperator(LV, Extra);
                //         }
                //         else
                //         {
                //             string LV = filterarray[j]["LV"].GetString();
                //             LV = LV.substr(1);
                //             Value Extra;
                //             Extra = filterarray[j]["EXTRA"].GetArray();
                //             CV = InOperator(LV, Extra);
                //         }
                //     }
                //     else
                //     { // lv는 인트타입의 상수
                //         int LV = filterarray[j]["LV"].GetInt();
                //         Value Extra;
                //         Extra = filterarray[j]["EXTRA"].GetArray();
                //         CV = InOperator(LV, Extra);
                //     }
                // }
                break;
            case IS: // NULL형식에 대한 확인 필요
                /* code */
                break;
            case ISNOT:
                /* code */
                break;
            case NOT:
                if (Passed)
                {
                    break;
                }
                else if (isnot)
                {
                    isnot = false;
                    // j++;
                }
                else
                {
                    isnot = true;
                    // j++;
                }
                /* code */
                break;
            case AND:
                isnot = false;
                if (CV == false)
                { // f and t or t and t
                    Passed = true;
                }
                else
                {
                    TmpV = CV;
                    // PrevOper = 1;
                }
                /* code */
                break;
            case OR:
                isnot = false;
                if (CV == true)
                {
                    isSaved = true;
                    // cout << "Saved or" << endl;
                    // SavedRow(Rawrowdata[i]);
                }
                else
                {
                    TmpV = true;
                    // PrevOper = 0;
                    Passed = false;
                }
                /* code */
                break;
            default:
                cout << "error this is no default" << endl;
                break;
            }
            // cout << CV << endl;
            if (isSaved == true)
            { // or을 통해 저장되었다면
                char *ptr = rowbuf;
                char *tmpsave = new char[newstartptr[ColName[ColNum - 1]] + newlengthraw[ColName[ColNum - 1]] - newstartptr[ColName[0]] + 2];
                memcpy(tmpsave, ptr + newstartptr[ColName[0]] - 2, newstartptr[ColName[ColNum - 1]] + newlengthraw[ColName[ColNum - 1]] - newstartptr[ColName[0]] + 2);
                SavedRow(tmpsave, newstartptr[ColName[ColNum - 1]] + newlengthraw[ColName[ColNum - 1]] - newstartptr[ColName[0]] + 2, filterresult, newstartptr[ColName[0]] - 2, newstartptr[ColName[ColNum - 1]] + newlengthraw[ColName[ColNum - 1]] - newstartptr[ColName[0]] + 2);
                // delete[] tmpsave;
                break;
            }
        }

        // }
        if (canSaved == true && isSaved == false && Passed != true && CV == true)
        { // and를 통해 저장된다면
            cout << "*Save Row*" << endl;
            char *ptr = rowbuf;
            char *tmpsave = new char[newstartptr[ColName[ColNum - 1]] + newlengthraw[ColName[ColNum - 1]] - newstartptr[ColName[0]] + 2];
            memcpy(tmpsave, ptr + newstartptr[ColName[0]] - 2, newstartptr[ColName[ColNum - 1]] + newlengthraw[ColName[ColNum - 1]] - newstartptr[ColName[0]] + 2);
            SavedRow(tmpsave, newstartptr[ColName[ColNum - 1]] + newlengthraw[ColName[ColNum - 1]] - newstartptr[ColName[0]] + 2, filterresult, newstartptr[ColName[0]] - 2, newstartptr[ColName[ColNum - 1]] + newlengthraw[ColName[ColNum - 1]] - newstartptr[ColName[0]] + 2);
            // delete[] tmpsave;
            // cout << tmpsave[0] << endl;
            //  cout << Rawrowdata[i] << endl;
        }

        std::cout << "  ------" << std::endl;
    }
    //sendfilterresult(filterresult);
    return 0;
}

void sendfilterresult(FilterResult filterresult){
    int sock;
    struct sockaddr_in serv_addr;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("10.0.5.38");
    serv_addr.sin_port = htons(8080);
    int len = sizeof(filterresult);
    bind(sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr));
    send(sock,(char*)&len, sizeof(int),0);
    send(sock,(char*)&filterresult,sizeof(filterresult),0);
}

bool LikeSubString(string lv, string rv)
{ // case 0, 1, 2, 3, 4 --> %sub%(문자열 전체) or %sub(맨 뒤 문자열) or sub%(맨 앞 문자열) or sub(똑같은지) or %s%u%b%(생각 필요)
    // 해당 문자열 포함 검색 * 또는 % 존재 a like 'asd'
    int len = rv.length();
    int LvLen = lv.length();
    std::string val;
    if (rv[0] == '%' && rv[len - 1] == '%')
    {
        // case 0
        val = rv.substr(1, len - 2);
        for (int i = 0; i < LvLen - len + 1; i++)
        {
            if (lv.substr(i, val.length()) == val)
            {
                return true;
            }
        }
    }
    else if (rv[0] == '%')
    {
        // case 1
        val = rv.substr(1, len - 1);
        if (lv.substr(lv.length() - val.length() - 1, val.length()) == val)
        {
            return true;
        }
    }
    else if (rv[len - 1] == '%')
    {
        // case 2
        val = rv.substr(0, len - 1);
        if (lv.substr(0, val.length()) == val)
        {
            return true;
        }
    }
    else
    {
        // case 3
        if (rv == lv)
        {
            return true;
        }
    }
    return false;
}

bool LikeSubString_v2(string lv, string rv)
{ // % 위치 찾기
    // 해당 문자열 포함 검색 * 또는 % 존재 a like 'asd'
    int len = rv.length();
    int LvLen = lv.length();
    int i = 0, j = 0;
    int substringsize = 0;
    bool isfirst = false, islast = false; // %가 맨 앞 또는 맨 뒤에 있는지에 대한 변수
    // cout << rv[0] << endl;
    if (rv[0] == '%')
    {
        isfirst = true;
    }
    if (rv[len - 1] == '%')
    {
        islast = true;
    }
    vector<string> val = split(rv, '%');
    // for (int k = 0; k < val.size(); k++){
    //     cout << val[k] << endl;
    // }
    // for(int k = 0; k < val.size(); k ++){
    //     cout << val[k] << endl;
    // }
    if (isfirst)
    {
        i = 1;
    }
    // cout << LvLen << " " << val[val.size() - 1].length() << endl;
    // cout << LvLen - val[val.size() - 1].length() << endl;
    for (i; i < val.size(); i++)
    {
        // cout << "print i : " << i << endl;

        for (j; j < LvLen - val[val.size() - 1].length() + 1; j++)
        { // 17까지 돌아야함 lvlen = 19 = 17
            // cout << "print j : " << j << endl;
            substringsize = val[i].length();
            if (!isfirst)
            {

                if (lv.substr(0, substringsize) != val[i])
                {
                    // cout << "111111" << endl;
                    return false;
                }
            }
            if (!islast)
            {

                if (lv.substr(LvLen - val[val.size() - 1].length(), val[val.size() - 1].length()) != val[val.size() - 1])
                {
                    // cout << lv.substr(LvLen - val[val.size()-1].length() + 1, val[val.size()-1].length()) << " " << val[val.size()-1] << endl;
                    // cout << "222222" << endl;
                    return false;
                }
            }
            if (lv.substr(j, val[i].length()) == val[i])
            {
                // cout << lv.substr(j,val[i].length()) << endl;
                if (i == val.size() - 1)
                {
                    // cout << lv.substr(j, val[i].length()) << " " << val[i] << endl;
                    return true;
                }
                else
                {
                    j = j + val[i].length();
                    i++;
                    continue;
                }
            }
        }
        return false;
    }

    return false;
}

bool InOperator(string lv, Value rv)
{
    // 여러 상수 or 연산 ex) a IN (50,60) == a = 50 or a = 60
    // std::string val = rv.substr(1, rv.length() - 2);
    // vector<string> RvVal = split(val, ',');
    // string tmparray[RvVal.size()];
    // copy(RvVal.begin(), RvVal.end(), tmparray);
    // for (int i = 0; i < RvVal.size(); i++)
    // {
    //     string RV = "";
    //     if (typedata[RvVal[i]] == 3 || typedata[RvVal[i]] == 14) //리틀에디안
    //     {
    //         RV = typeLittle(typedata, tmparray, i, newlengthraw, newstartptr, rowbuf, lvtype);
    //         // cout << "type little" << endl;
    //         //나중 다른 데이트 처리를 위한 구분
    //     }

    //     else if (typedata[RvVal[i]] == 254 || typedata[RvVal[i]] == 15) //빅에디안
    //     {
    //         RV = typeBig(newlengthraw, newstartptr, tmparray, i, rowbuf, lvtype);
    //         // cout << "type big" << endl;
    //     }
    //     else if (typedata[RvVal[i]] == 246) //예외 Decimal일때
    //     {
    //         RV = typeDecimal(newlengthraw, newstartptr, tmparray, i, rowbuf, lvtype);
    //         // cout << "type decimal" << endl;
    //     }
    //     else
    //     {
    //         RV = RvVal[i];
    //     }
    //     if (lv == RvVal[i])
    //     {
    //         return true;
    //     }
    // }
    return false;
}
bool InOperator(int lv, Value rv)
{
    // for (int i = 0; i < rv.Size(); i ++){

    // }
    return false;
}

bool BetweenOperator(int lv, int rv1, int rv2)
{
    // a between 10 and 20 == a >= 10 and a <= 20
    if (lv >= rv1 && lv <= rv2)
    {
        return true;
    }
    return false;
}

bool BetweenOperator(string lv, string rv1, string rv2)
{
    // a between 10 and 20 == a >= 10 and a <= 20
    if (lv >= rv1 && lv <= rv2)
    {
        return true;
    }
    return false;
}

bool IsOperator(string lv, string rv, int isnot)
{
    // a is null or a is not null
    if (lv.empty())
    {
        if (isnot == 0)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    if (isnot == 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

void SavedRow(char *row, int length, FilterResult &filterresult, int startlength, int nowlength)
{
    cout << "[Saved Row(HEX)] VALUE: ";
    // for (int k = 0; k < length; k++)
    // {
    //     cout << hex << (int)row[k];
    // }
    filterresult.offset[filterresult.rows] = startlength;
    memcpy(filterresult.data + startlength, row, nowlength);
    filterresult.totallength = startlength + nowlength;
    filterresult.rows++;
    cout << endl;
    // sendrow.push_back(row[testsmall_line_col[0]] + "," + row[testsmall_line_col[1]] + "," + row[testsmall_line_col[3]] + "," + row[testsmall_line_col[3]] + "," + row[testsmall_line_col[4]] + "," + row[testsmall_line_col[5]] + "," + row[testsmall_line_col[6]] + "," + row[testsmall_line_col[7]] + "," + row[testsmall_line_col[8]] + "," + row[testsmall_line_col[9]] + "," + row[testsmall_line_col[10]] + "," + row[testsmall_line_col[11]] + "," + row[testsmall_line_col[12]] + "," + row[testsmall_line_col[13]] + "," + row[testsmall_line_col[14]] + "," + row[testsmall_line_col[15]] );
}

vector<string> split(string str, char Delimiter)
{
    istringstream iss(str); // istringstream에 str을 담는다.
    string buffer;          // 구분자를 기준으로 절삭된 문자열이 담겨지는 버퍼

    vector<string> result;

    // istringstream은 istream을 상속받으므로 getline을 사용할 수 있다.
    while (getline(iss, buffer, Delimiter))
    {
        result.push_back(buffer); // 절삭된 문자열을 vector에 저장
    }

    return result;
}

bool isvarc(int datatype[], int ColNum)
{
    int isvarchar = 0;
    for (int i = 0; i < ColNum; i++) // varchar 확인
    {
        if (datatype[i] == 15)
        {
            isvarchar = 1;
        }
    }
    return isvarchar;
}

void makedefaultmap(string ColName[], int startoff[], int offlen[], int datatype[], int ColNum, unordered_map<string, int> &startptr, unordered_map<string, int> &lengthRaw, unordered_map<string, int> &typedata)
{
    for (int i = 0; i < ColNum; i++)
    {
        startptr.insert(make_pair(ColName[i], startoff[i]));
        lengthRaw.insert(make_pair(ColName[i], offlen[i]));
        typedata.insert(make_pair(ColName[i], datatype[i]));
    }
}

void makenewmap(int isvarchar, int ColNum, unordered_map<string, int> &newstartptr, unordered_map<string, int> &newlengthraw, int datatype[], unordered_map<string, int> lengthRaw, string ColName[], int &iter, int startoff[], int offlen[], char *rowbuf)
{
    bool aftervarchar = 0;
    int rowlength = 0;
    if (isvarchar == 1)
    {
        newstartptr.clear();
        newlengthraw.clear();
        for (int j = 0; j < ColNum; j++)
        {
            int newofflen = 0;
            if (datatype[j] == 15 || aftervarchar == 1)
            {
                aftervarchar = 1;
                if (datatype[j] == 15)
                { //맨 앞 컬럼 타입이 varchar일 경우 길이를 새로 구하고 그 길이로 시작 인덱스를 구하는데, 그 이후의 모든 시작인덱스를 구해줘야함
                    if (lengthRaw[ColName[j]] < 256)
                    { // varchar 길이 1바이트
                        if (j == 0)
                        {
                            newofflen = (int)rowbuf[iter + startoff[j]];
                            newstartptr.insert(make_pair(ColName[j], iter + startoff[j] + 1));
                            newlengthraw.insert(make_pair(ColName[j], newofflen));
                        }
                        else
                        {
                            // cout << newstartptr[testsmall_line_col[j-1]] + newlengthraw[testsmall_line_col[j-1]] << endl;
                            newofflen = (int)rowbuf[newstartptr[ColName[j - 1]] + newlengthraw[ColName[j - 1]]];
                            newstartptr.insert(make_pair(ColName[j], newstartptr[ColName[j - 1]] + newlengthraw[ColName[j - 1]] + 1));
                            newlengthraw.insert(make_pair(ColName[j], newofflen));
                            // cout << "newofflen = " << (int)rowbuf[iter + newstartptr[testsmall_line_col[j-1]] + newlengthraw[testsmall_line_col[j-1]]] << endl;
                        }
                    }
                    else if (lengthRaw[ColName[j]] >= 256)
                    {
                        // varchar 길이 2바이트
                        char lenbuf[4];
                        lenbuf[2] = 0x00;
                        lenbuf[3] = 0x00;
                        int *lengthtmp;
                        if (j == 0)
                        {
                            for (int k = 0; k < 2; k++)
                            {
                                lenbuf[k] = rowbuf[iter + startoff[j] + k];
                                lengthtmp = (int *)lenbuf;
                            }
                            newofflen = lengthtmp[0];
                            newstartptr.insert(make_pair(ColName[j], iter + startoff[j] + 2));
                            newlengthraw.insert(make_pair(ColName[j], newofflen));
                        }
                        else
                        {
                            // cout << newstartptr[testsmall_line_col[j-1]] + newlengthraw[testsmall_line_col[j-1]] << endl;
                            for (int k = 0; k < 2; k++)
                            {
                                lenbuf[k] = rowbuf[newstartptr[ColName[j - 1]] + newlengthraw[ColName[j - 1]] + k];
                                lengthtmp = (int *)lenbuf;
                            }
                            newofflen = lengthtmp[0];
                            newstartptr.insert(make_pair(ColName[j], newstartptr[ColName[j - 1]] + newlengthraw[ColName[j - 1]] + 2));
                            newlengthraw.insert(make_pair(ColName[j], newofflen));
                            // cout << "newofflen = " << (int)rowbuf[iter + newstartptr[testsmall_line_col[j-1]] + newlengthraw[testsmall_line_col[j-1]]] << endl;
                        }
                    }
                }
                else
                {
                    newstartptr.insert(make_pair(ColName[j], newstartptr[ColName[j - 1]] + newlengthraw[ColName[j - 1]]));
                    newlengthraw.insert(make_pair(ColName[j], offlen[j]));
                }
            }
            else
            {
                newstartptr.insert(make_pair(ColName[j], iter + startoff[j]));
                // cout << iter + startoff[j] << endl;
                newlengthraw.insert(make_pair(ColName[j], offlen[j]));
            }
        }
        // cout << newstartptr[testsmall_line_col[ColNum - 1]] << " " << testsmall_line_col[ColNum - 1] << endl;
        // iter = newstartptr[ColName[ColNum - 1]] + newlengthraw[ColName[ColNum - 1]];
        // cout << iter << endl;
    }
    else
    {
        for (int j = 0; j < ColNum; j++)
        {
            newstartptr.insert(make_pair(ColName[j], iter + startoff[j]));
            newlengthraw.insert(make_pair(ColName[j], offlen[j]));
        }
        // newstartptr = startptr;
        // newlengthraw = lengthRaw;
        // rowlength = startoff[ColNum - 1] + offlen[ColNum - 1];
    }
    iter = newstartptr[ColName[ColNum - 1]] + newlengthraw[ColName[ColNum - 1]];
}

void compareGE(string LV, string RV, bool &CV, bool &TmpV, bool &canSaved, bool isnot)
{
    if (LV >= RV)
    {
        // cout << "LV is ge" << endl;
        // cout << LV << " " << OneRow[WhereClauses[j+1]] << endl;
        if (TmpV == true)
        {
            CV = true;
            canSaved = true;
        }
        else
        {
            CV = false;
            canSaved = false;
        }
    }
    else
    {
        CV = false;
        canSaved = false;
    }
    if (isnot)
    {
        if (CV)
        {
            CV = false;
            canSaved = false;
        }
        else
        {
            CV = true;
            canSaved = true;
        }
    }
}

void compareGE(int LV, int RV, bool &CV, bool &TmpV, bool &canSaved, bool isnot)
{
    if (LV >= RV)
    {
        // cout << "LV is ge" << endl;
        // cout << LV << " " << OneRow[WhereClauses[j+1]] << endl;
        if (TmpV == true)
        {
            CV = true;
            canSaved = true;
        }
        else
        {
            CV = false;
            canSaved = false;
        }
    }
    else
    {
        CV = false;
        canSaved = false;
    }
    if (isnot)
    {
        if (CV)
        {
            CV = false;
            canSaved = false;
        }
        else
        {
            CV = true;
            canSaved = true;
        }
    }
}

void compareLE(string LV, string RV, bool &CV, bool &TmpV, bool &canSaved, bool isnot)
{
    if (LV <= RV)
    {
        if (TmpV == true)
        {
            CV = true;
            canSaved = true;
        }
        else
        {
            CV = false;
            canSaved = false;
        }
    }
    else
    {
        CV = false;
        canSaved = false;
    }
    if (isnot)
    {
        if (CV)
        {
            CV = false;
            canSaved = false;
        }
        else
        {
            CV = true;
            canSaved = true;
        }
    }
}
void compareLE(int LV, int RV, bool &CV, bool &TmpV, bool &canSaved, bool isnot)
{
    if (LV <= RV)
    {
        if (TmpV == true)
        {
            CV = true;
            canSaved = true;
        }
        else
        {
            CV = false;
            canSaved = false;
        }
    }
    else
    {
        CV = false;
        canSaved = false;
    }
    if (isnot)
    {
        if (CV)
        {
            CV = false;
            canSaved = false;
        }
        else
        {
            CV = true;
            canSaved = true;
        }
    }
}
void compareGT(string LV, string RV, bool &CV, bool &TmpV, bool &canSaved, bool isnot)
{
    if (LV > RV)
    {
        if (TmpV == true)
        {
            CV = true;
            canSaved = true;
        }
        else
        {
            CV = false;
            canSaved = false;
        }
    }
    else
    {
        CV = false;
        canSaved = false;
    }
    if (isnot)
    {
        if (CV)
        {
            CV = false;
            canSaved = false;
        }
        else
        {
            CV = true;
            canSaved = true;
        }
    }
}
void compareGT(int LV, int RV, bool &CV, bool &TmpV, bool &canSaved, bool isnot)
{
    if (LV > RV)
    {
        if (TmpV == true)
        {
            CV = true;
            canSaved = true;
        }
        else
        {
            CV = false;
            canSaved = false;
        }
    }
    else
    {
        CV = false;
        canSaved = false;
    }
    if (isnot)
    {
        if (CV)
        {
            CV = false;
            canSaved = false;
        }
        else
        {
            CV = true;
            canSaved = true;
        }
    }
}
void compareLT(string LV, string RV, bool &CV, bool &TmpV, bool &canSaved, bool isnot)
{

    if (LV < RV)
    {
        // cout << "LV is small" << endl;
        if (TmpV == true)
        {
            CV = true;
            canSaved = true;
        }
        else
        {
            CV = false;
            canSaved = false;
        }
    }
    else
    {
        CV = false;
        canSaved = false;
    }
    if (isnot)
    {
        if (CV)
        {
            CV = false;
            canSaved = false;
        }
        else
        {
            CV = true;
            canSaved = true;
        }
    }
}
void compareLT(int LV, int RV, bool &CV, bool &TmpV, bool &canSaved, bool isnot)
{

    if (LV < RV)
    {
        // cout << "LV is small" << endl;
        if (TmpV == true)
        {
            CV = true;
            canSaved = true;
        }
        else
        {
            CV = false;
            canSaved = false;
        }
    }
    else
    {
        CV = false;
        canSaved = false;
    }
    if (isnot)
    {
        if (CV)
        {
            CV = false;
            canSaved = false;
        }
        else
        {
            CV = true;
            canSaved = true;
        }
    }
}
void compareET(string LV, string RV, bool &CV, bool &TmpV, bool &canSaved, bool isnot)
{

    if (LV == RV)
    {
        // cout << "same" << endl;
        if (TmpV == true)
        {
            CV = true;
            canSaved = true;
        }
        else
        {
            CV = false;
            canSaved = false;
        }
    }
    else
    {
        CV = false;
        canSaved = false;
    }
    if (isnot)
    {
        if (CV)
        {
            CV = false;
            canSaved = false;
        }
        else
        {
            CV = true;
            canSaved = true;
        }
    }
}
void compareET(int LV, int RV, bool &CV, bool &TmpV, bool &canSaved, bool isnot)
{

    if (LV == RV)
    {
        // cout << "same" << endl;
        if (TmpV == true)
        {
            CV = true;
            canSaved = true;
        }
        else
        {
            CV = false;
            canSaved = false;
        }
    }
    else
    {
        CV = false;
        canSaved = false;
    }
    if (isnot)
    {
        if (CV)
        {
            CV = false;
            canSaved = false;
        }
        else
        {
            CV = true;
            canSaved = true;
        }
    }
}

void compareNE(string LV, string RV, bool &CV, bool &TmpV, bool &canSaved, bool isnot)
{

    if (LV != RV)
    {
        if (TmpV == true)
        {
            CV = true;
            canSaved = true;
        }
        else
        { //의미 없음 tmpv 가 false일 경우는 passed
            CV = false;
            canSaved = false;
        }
    }
    else
    {
        CV = false;
        canSaved = false;
    }
    if (isnot)
    {
        if (CV)
        {
            CV = false;
            canSaved = false;
        }
        else
        {
            CV = true;
            canSaved = true;
        }
    }
}
void compareNE(int LV, int RV, bool &CV, bool &TmpV, bool &canSaved, bool isnot)
{

    if (LV != RV)
    {
        if (TmpV == true)
        {
            CV = true;
            canSaved = true;
        }
        else
        { //의미 없음 tmpv 가 false일 경우는 passed
            CV = false;
            canSaved = false;
        }
    }
    else
    {
        CV = false;
        canSaved = false;
    }
    if (isnot)
    {
        if (CV)
        {
            CV = false;
            canSaved = false;
        }
        else
        {
            CV = true;
            canSaved = true;
        }
    }
}

int typeLittle(unordered_map<string, int> typedata, string colname, unordered_map<string, int> newlengthraw, unordered_map<string, int> newstartptr, char *rowbuf, int &lvtype)
{
    if (typedata[colname] == 14)
    { // date
        // cout << 1 << endl;
        int *tmphex;
        char temphexbuf[4];
        //  = new char[4];
        int retint;
        temphexbuf[0] == 0x00;
        temphexbuf[1] == 0x00;
        temphexbuf[2] == 0x00;
        temphexbuf[3] == 0x00;
        // cout << "col length : " << newlengthraw[colname] << endl;
        for (int k = 0; k < newlengthraw[colname]; k++)
        {
            // cout << rowbuf[newstartptr[colname] + k] << endl;
            temphexbuf[k] = rowbuf[newstartptr[colname] + k];
        }
        tmphex = (int *)temphexbuf;
        retint = tmphex[0];
        // cout << "tmphex : " << retint << endl;
        // delete[] temphexbuf;
        lvtype = 14;
        return retint;
    }
    else if (typedata[colname] == 3)
    { // int
        char intbuf[4];
        //  = new char[4];
        int *intbuff;
        int retint;
        intbuf[0] == 0x00;
        intbuf[1] == 0x00;
        intbuf[2] == 0x00;
        intbuf[3] == 0x00;
        for (int k = 0; k < newlengthraw[colname]; k++)
        {
            intbuf[k] = rowbuf[newstartptr[colname] + k];
        }
        intbuff = (int *)intbuf;
        lvtype = 3;
        retint = intbuff[0];
        // delete[] intbuf;
        //  cout << intbuff[0] << endl;
        return retint;
    }
    return 0;
    // else
    // {
    //     //예외 타입
    //     return NULL;
    // }
}

string typeBig(unordered_map<string, int> newlengthraw, unordered_map<string, int> newstartptr, string colname, char *rowbuf, int &lvtype)
{
    string tmpstring = "";
    for (int k = 0; k < newlengthraw[colname]; k++)
    {
        tmpstring = tmpstring + (char)rowbuf[newstartptr[colname] + k];
    }
    lvtype = 0;
    return tmpstring;
}
string typeDecimal(unordered_map<string, int> newlengthraw, unordered_map<string, int> newstartptr, string colname, char *rowbuf, int &lvtype)
{
    string tmpstring = "";
    for (int k = 0; k < newlengthraw[colname]; k++)
    {
        ostringstream oss;
        int *tmpdata;
        char *tmpbuf = new char[4];
        tmpbuf[0] = 0x80;
        tmpbuf[1] = 0x00;
        tmpbuf[2] = 0x00;
        tmpbuf[3] = 0x00;
        tmpbuf[0] = rowbuf[newstartptr[colname] + k];
        tmpdata = (int *)tmpbuf;
        oss << hex << tmpdata[0];
        // oss << hex << rowbuf[newstartptr[WhereClauses[j]] + k];
        if (oss.str().length() <= 1)
        {
            tmpstring = tmpstring + "0" + oss.str();
        }
        else
        {
            tmpstring = tmpstring + oss.str();
        }
        delete[] tmpbuf;
    }
    lvtype = 0;

    return tmpstring;
}

string ItoDec(int inum)
{
    std::stringstream ss;
    std::string s;
    ss << hex << inum;
    s = ss.str();
    string decimal = "80";
    for (int i = 0; i < 10 - s.length(); i++)
    {
        decimal = decimal + "0";
    }
    decimal = decimal + s;
    decimal = decimal + "00";
    return decimal;
}