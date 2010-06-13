// stdlib:
#include <stdio.h>
#include <cstring>
#include <math.h>
#include <stdlib.h>

// STL:
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
using namespace std;

// OpenCV:
#include <cv.h>
#include <highgui.h>

int encode(int state,int color,int element,int N_COLORS)
{
	return state*N_COLORS*3 + color*3 + element;
}

int main()
{
    // ---------------- things a casual user will want to experiment with -----------------------

    // choose the type of turmite you want to search for:
    const int N_STATES=2;
    const int N_COLORS=3;
    const bool relative_movement = true; // true: relative Turmites ("TurNing machines"), false: absolute Turmites (Turing machines)

    // specify some constraints we need to help us search
    const int ITS=60000; // Limitation of this approach: if BB lasts longer than this we'll miss it
    const int R=50; // square radius. Limitation: if BB spreads more than this in any direction we'll miss it

    const unsigned long long PRINT_EVERY=1000; // how often to report back

    // ------------------------------------------------------------------------------------------

    const int N_DIM=2; 
    
    const int SIDE=2*R+1;

    const int N_DIRS=6;
    const int N_MOVES=1+6;
	int DIRS[N_DIRS][N_DIM] = {{0,-1},{1,-1},{1,0},{0,1},{-1,1},{-1,0}}; // following Golly, we skip SE and NW
    
    // for output, what is the label for each direction
    const string DIR_TEXT_ABSOLUTE[N_MOVES] = {"''","'A'","'B'","'C'","'D'","'E'","'F'"};
    // for output, what is the label for each turn
    const string TURN_TEXT_RELATIVE[N_MOVES] = 
        {"0","1","2","4","8","16","32"}; // 0=halt, 1=noturn, 2=left, 4=right, 8=back-left, 16=back-right, 32=u-turn
    const int DIR_AFTER_TURN[N_MOVES][N_MOVES] = // new_dir = DIR_AFTER_TURN[turn][old_dir]
        {{0,0,0,0,0,0,0},{0,1,2,3,4,5,6},{0,6,1,2,3,4,5},{0,2,3,4,5,6,1},{0,5,6,1,2,3,4},
        {0,3,4,5,6,1,2},{0,4,5,6,1,2,3}};
    string DIR_TEXT[N_MOVES];
    for(int i=0;i<N_MOVES;i++) 
        DIR_TEXT[i] = relative_movement?TURN_TEXT_RELATIVE[i]:DIR_TEXT_ABSOLUTE[i];

    const unsigned int N_CELLS = (int)pow((float)SIDE,N_DIM);
    vector<unsigned char> grid;
    try {
        grid.resize(N_CELLS);
    }
    catch(...)
    {
        cout << "Grid too large to be allocated. Reduce the value of R." << endl;
        exit(1);
    }

	const int HEX_SIDE = 20;
    IplImage *image = cvCreateImage(cvSize(HEX_SIDE*2*SIDE,HEX_SIDE*2*SIDE),8,1);

    unsigned char turmite[N_STATES*N_COLORS*3]; // turmite[i] is an index into possible_entries[i]
    vector<vector<unsigned char> > possible_entries(N_STATES*N_COLORS*3);

    int ts,t_pos[N_DIM],t_dir;
    unsigned char color,new_color,new_dir;
    bool halted;
    int its,max_its=-1;
    int n_nonzero,max_nonzero=-1;

    unsigned long long tried=0,tested=0,until_print=PRINT_EVERY;

    int iEntry,iState,iColor,n_halts,iDim,iCell;
    bool satisfied,off_grid;

    ostringstream oss;
    oss << "found_hex_" << N_DIM << "d_";
    if(relative_movement)
        oss << "relative_";
    else
        oss << "absolute_";
    oss << N_STATES << "s_" << N_COLORS << "c.txt";
    ofstream out(oss.str().c_str());

    cout << "Saving results to: " << oss.str() << endl;

    // initialize the turmite and fix some entries
    memset(&turmite[0],0,sizeof(unsigned char)*N_STATES*N_COLORS*3);
    for(int iState=0;iState<N_STATES;iState++)
    {
        for(int iColor=0;iColor<N_COLORS;iColor++)
        {
            for(int i=0;i<N_COLORS;i++)
                possible_entries[encode(iState,iColor,0,N_COLORS)].push_back(i);
            if(iState<=2)
            {
                for(int i=0;i<N_MOVES;i++)
                    possible_entries[encode(iState,iColor,1,N_COLORS)].push_back(i);
            }
            else
            {
                // no halt state for states >2 (by symmetry)
                for(int i=1;i<N_MOVES;i++)
                    possible_entries[encode(iState,iColor,1,N_COLORS)].push_back(i);
            }
            for(int i=0;i<N_STATES;i++)
                possible_entries[encode(iState,iColor,2,N_COLORS)].push_back(i);
        }
    }
    /*if(relative_movement)
    {
        // first color printed can only be 0 or 1 by symmetry
        possible_entries[0].clear();
        possible_entries[0].push_back(0);
        possible_entries[0].push_back(1);
        // first move can only be F, U, R, or BR (not L or H) by symmetry
        possible_entries[1].clear();
        possible_entries[1].push_back(1);
        possible_entries[1].push_back(3);
        possible_entries[1].push_back(5);
        possible_entries[1].push_back(6);
        // first state can be 0 or 1 (not higher) by symmetry
        possible_entries[2].clear();
        possible_entries[2].push_back(0);
        possible_entries[2].push_back(1);
    }
    else
    {
        // first triple is fixed at {1,'A',1} because of symmetry
        // -if first print is 0 then can just take the destination state as the starting one)
        // -if first print is >1 then by rotating the colors around can make it 1
        // -if first transition is to state 0 then turmite will zip off
        // -if first transition is to state >1 then by rotating the states can make it 1
        // -if first move is anything else, can just rotate it round
        possible_entries[0].clear();
        possible_entries[0].push_back(1);
        possible_entries[1].clear();
        possible_entries[1].push_back(1);
        possible_entries[2].clear();
        possible_entries[2].push_back(1);
    }*/

    // compute how far we've got to go
    unsigned long long target=1;
    for(int iEntry=0;iEntry<3*N_STATES*N_COLORS;iEntry++)
        target *= possible_entries[iEntry].size();
    out << "Total number of machines: " << target << endl;
    cout << "Total number of machines: " << target << endl;

	// DEBUG: start with a specific turmite
	{
		// {{{1,16,0},{1,8,1}},{{1,8,2},{1,16,0}},{{1,0,0},{0,1,0}}}
		//unsigned char t57867[] = {0,5,0,1,4,1,1,4,2,1,5,0,1,0,0,0,1,0}; // will be incremented

		//{{{0,16,1},{2,16,1},{1,0,0}},{{1,1,0},{1,8,1},{1,1,0}}}
		//unsigned char t44438[] = {2,4,1,2,5,1,1,0,0,1,1,0,1,4,1,1,1,0};

		//{{{1,16,1},{1,1,0},{1,8,0}},{{1,16,0},{2,8,0},{1,0,0}}}
		unsigned char t2893[] = {0,5,1,1,1,0,1,4,0,1,5,0,2,4,0,1,0,0};
		for(int iEntry=0;iEntry<3*N_STATES*N_COLORS;iEntry++)
			turmite[iEntry] = find(possible_entries[iEntry].begin(),
				possible_entries[iEntry].end(),t2893[iEntry])
				-possible_entries[iEntry].begin();
	}

    // count the number of halts in the initial turmite
    n_halts=0;
    for(iEntry=1;iEntry<N_STATES*N_COLORS*3;iEntry+=3)
    {
        if(possible_entries[iEntry][turmite[iEntry]]==0)
            n_halts++;
    }

    bool finished = false;
    while(!finished)
    {
         // increment the turmite
        satisfied = false;
        do {
            for(iEntry=0;iEntry<3*N_STATES*N_COLORS;iEntry++)
            {
                if(turmite[iEntry] < possible_entries[iEntry].size()-1)
                {
                    if(iEntry%3==1 && possible_entries[iEntry][turmite[iEntry]]==0)
                        n_halts--;
                    turmite[iEntry]++;
                    break;
                }
                else
                {
                    turmite[iEntry]=0;
                    if(iEntry%3==1 && possible_entries[iEntry][turmite[iEntry]]==0) n_halts++;
                }
            }
            if(iEntry == 3*N_STATES*N_COLORS)
            {
                finished = true; // we've tried every turmite
                break;
            }
            tried++;
            if(n_halts!=1) continue; // keep working through the possibilities
            // the halt triple should be {1,0,0}
            for(iEntry=1;iEntry<N_STATES*N_COLORS*3;iEntry+=3)
            {
                if(possible_entries[iEntry][turmite[iEntry]]==0)
                {
                    // this is the halt triple (we know there's only one)
                    // is it {1,0,0}?
                    if(possible_entries[iEntry-1][turmite[iEntry-1]]==1 &&
                        possible_entries[iEntry+1][turmite[iEntry+1]]==0)
                    {
                        satisfied=true;
                        break;
                    }
                }
            }
        } while(!satisfied);
        if(finished) break;
        // test the turmite
        grid.assign(N_CELLS,0); // clear the grid
        for(iDim=0;iDim<N_DIM;iDim++) t_pos[iDim] = R; // start in the middle
        ts = 0; // start in state 0 (symmetry constraint)
        if(relative_movement)
            t_dir = 1; // starting orientation (arbitrary)
        halted=false;
        n_nonzero=0;
        for(its=0;its<ITS;its++)
        {
            iCell = t_pos[0];
            for(iDim=1;iDim<N_DIM;iDim++) iCell = iCell*SIDE + t_pos[iDim];
            color = grid[iCell];
            if(!relative_movement)
                new_dir = possible_entries[encode(ts,color,1,N_COLORS)][turmite[encode(ts,color,1,N_COLORS)]];
            else
                new_dir = DIR_AFTER_TURN[possible_entries[encode(ts,color,1,N_COLORS)][turmite[encode(ts,color,1,N_COLORS)]]][t_dir];
            new_color = possible_entries[encode(ts,color,0,N_COLORS)][turmite[encode(ts,color,0,N_COLORS)]];
            if(color!=new_color)
            {
                grid[iCell] = new_color; // cell changes color
                if(color==0) n_nonzero++;
                else if(new_color==0) n_nonzero--;
            }
            if(new_dir==0) // halted
            {
                halted=true;
                its++; // want the number of steps to include the halt step
                break;
            }
            off_grid = false;
            for(iDim=0;iDim<N_DIM;iDim++)
            {
                t_pos[iDim] += DIRS[new_dir][iDim];
                if(t_pos[iDim]<0 || t_pos[iDim]>=SIDE)
                {
                    off_grid=true;
                    break;
                }
            }
            if(off_grid)
            {
                // turmite has moved off the grid
                // we say it moved too fast: not interesting
                break;
            }
            ts = possible_entries[encode(ts,color,2,N_COLORS)][turmite[encode(ts,color,2,N_COLORS)]]; // turmite adopts new state
            if(relative_movement)
                t_dir = new_dir; // turmite adopts new orientation
        }
        if(halted)
        {
            // is it a new record?
            if(its>max_its || n_nonzero>max_nonzero)
            {
                if(its>max_its)
                {
                    max_its = its;
                    out << "New steps record:\n";
                }
                if(n_nonzero>max_nonzero)
                {
                    max_nonzero = n_nonzero;
                    out << "New high score:\n";
                }
                out << its << " (popn. " << n_nonzero << "): {";
                for(iState=0;iState<N_STATES;iState++)
                {
                    if(iState>0)
                        out << ",";
                    out << "{";
                    for(iColor=0;iColor<N_COLORS;iColor++)
                    {
                        if(iColor>0)
                            out << ",";
                        out << "{";
                        out << (int)possible_entries[encode(iState,iColor,0,N_COLORS)][turmite[encode(iState,iColor,0,N_COLORS)]] << ",";
                        out << DIR_TEXT[possible_entries[encode(iState,iColor,1,N_COLORS)][turmite[encode(iState,iColor,1,N_COLORS)]]] << ",";
                        out << (int)possible_entries[encode(iState,iColor,2,N_COLORS)][turmite[encode(iState,iColor,2,N_COLORS)]];
                        out << "}";
                    }
                    out << "}";
                }
                out << "}" << endl;
                if(true)
                {
                    // also save the image
					cvSet(image,cvScalar(255));
					CvPoint **pts = new CvPoint*[1];
					const int npts=6;
					pts[0] = new CvPoint[npts];
					uchar state,col;
					float px,py;
					float h = HEX_SIDE * sqrt(3.0)/2.0;
					float HALF_SIDE = HEX_SIDE/2;
					for(int y=0;y<SIDE;y++)
					{
						for(int x=0;x<SIDE;x++)
						{
							state = grid[x*SIDE+y];
							if(state>0)
							{
								// draw triangle
								px = x*h*2 + h*y;
								py = y*HEX_SIDE*1.5;
								pts[0][0] = cvPoint(cvRound(px),cvRound(py-HEX_SIDE));
								pts[0][1] = cvPoint(cvRound(px+h),cvRound(py-HALF_SIDE));
								pts[0][2] = cvPoint(cvRound(px+h),cvRound(py+HALF_SIDE));
								pts[0][3] = cvPoint(cvRound(px),cvRound(py+HEX_SIDE));
								pts[0][4] = cvPoint(cvRound(px-h),cvRound(py+HALF_SIDE));
								pts[0][5] = cvPoint(cvRound(px-h),cvRound(py-HALF_SIDE));
								col = 255 - 255 * state / (N_COLORS-1);
								cvFillConvexPoly(image,pts[0],npts,cvScalar(col,col,col));
								cvPolyLine(image,pts,&npts,1,1,cvScalar(255,255,255),2,CV_AA);
							}
						}
					}
					delete []pts[0];
					delete []pts;
                    char fn[1000];
                    sprintf(fn,"hex_%d-%d_%dsteps_%dcells.png",N_STATES,N_COLORS,its,n_nonzero);
                    cvSaveImage(fn,image);
                }
            }
        }
        tested++;
        until_print--;
        if(until_print==0)
        {
            cout << "Tried: " << tried << " (" << 100*(tried/(float)target) << "%) Tested: " << tested << " Best steps: " << max_its << " Best score: " << max_nonzero << endl;
            until_print=PRINT_EVERY;
        }
    }
    out << "Run completed. If better machines exist then they take more than " << ITS << " steps or move more than " << R << " squares from the starting position." << endl;
}
