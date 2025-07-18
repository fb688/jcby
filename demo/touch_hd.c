//获取点击坐标
void get_xy(int *x ,int *y)
{
    *x = *y = -1;

    //定义输入事件结构体
    struct input_event buf;

    while (1)
    {
        //没有点击时会阻塞在这个循环中
        read (fd_touch , &buf , sizeof buf );

        if(buf.type == EV_ABS) //是触摸屏事件
        {
            if(buf.code == ABS_X)   //横坐标
            {
                *x = buf.value;
                //*x = *x *800/1024;  //黑色屏幕
            }
            else if (buf.code == ABS_Y) //纵坐标
            {
                *y = buf.value;
                //*y = *y*480/600;   //黑色屏幕

                //if(*x >= 0) 
                  //  break;
            }
        }
        if(buf.type == EV_KEY && buf.code == BTN_TOUCH && buf.value == 0)   //松开
            break;
    }
}


enum  {UP , DOWN , LIFT ,RIGHT , CLICK};

//获取点击和滑动
int read_Touch(int *x ,int *y)
{
    *x = *y = -1;
    int start_x,start_y,end_x,end_y;
    //定义输入事件结构体
    struct input_event buf;

    while (1)
    {
        read (fd_touch , &buf , sizeof buf );

        if(buf.type == EV_ABS) //是触摸屏事件
        {
            if(buf.code == ABS_X)   //横坐标
            {
                *x = buf.value;
                *x = *x *800/1024;  //黑色屏幕
            }
            else if (buf.code == ABS_Y) //纵坐标
            {
                *y = buf.value;
                *y = *y*480/600;   //黑色屏幕
            }
        }
        if(buf.type == EV_KEY && buf.code == BTN_TOUCH)
        {
            if(buf.value == 0)//松开
            {
                //记录终点坐标
                end_x = *x;
                end_y = *y;
                break;
            }
            else if (buf.value == 1)    //按下
            {
                //记录起点坐标
                start_x = *x;
                start_y = *y;
            }
        }
    }

    //终点坐标 减 起点坐标
    int temp_x = end_x - start_x;
    int temp_y = end_y - start_y;

    //区分是点击还是滑动
    if(abs(temp_x) < 20 && abs(temp_y) <20)
    {
        //点击
        return CLICK;
    }
    else
    {
        if(abs(temp_x) > abs(temp_y))
        {
            //左右滑动
            if(temp_x > 0)
            {
                return RIGHT;
            }
            else
            {
                return LIFT;
            }
            
        }
        else
        {
            //上下滑动
            if(temp_y > 0)
                return DOWN;
            else
                return UP;
        }
    }
    
}