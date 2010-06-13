// stdlib:
#include <math.h>

// STL:
#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
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
    // ------ user parameters ----------------------------------
    const int N_STATES = 2;
    const int N_COLORS = 2;
    const int R = 200; // square radius
    const int ITS = 100000;
	const int PRINT_EVERY = 100;
    // ---------------------------------------------------------
    
    const int N_DIM = 2;
    const int N_TURNS = 4; // 0=halt, 1=right, 2=left, 3=u-turn
	const string TURN_TEXT[4] = {"0","2","1","4"};
    const int DIR_AFTER_TURN[4][4] = // new_dir = DIR_AFTER_TURN[old_dir][turn]
        {{0,0,0,0},{0,3,2,1},{0,1,3,2},{0,2,1,3}};

	const int UP_TURN[4][4][2] = // triangle pointing up, dx,dy = UP_TURN[turn][current_dir][xy]
		{ {{0,0},{0,0},{0,0},{0,0}}, // no point moving if turn=halt
		{{0,0},{1,0},{0,1},{-1,0}}, // right
		{{0,0},{-1,0},{1,0},{0,1}}, // left
		{{0,0},{0,1},{-1,0},{1,0}} }; // u-turn
	const int DOWN_TURN[4][4][2] = // triangle pointing down, dx,dy = UP_TURN[turn][current_dir][xy]
	  { {{0,0},{0,0},{0,0},{0,0}}, // no point moving if turn=halt
		{{0,0},{-1,0},{0,-1},{1,0}}, // right
		{{0,0},{1,0},{-1,0},{0,-1}}, // left
		{{0,0},{0,-1},{1,0},{-1,0}} }; // u-turn
    
    const int SIDE = 2*R+1;
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

	const int TRI_BASE=30;
	const int TRI_HEIGHT = TRI_BASE * sqrt(3.0)/2.0;
    IplImage *image = cvCreateImage(cvSize(TRI_BASE*SIDE/2,TRI_HEIGHT*SIDE),8,1);

    int t_pos[N_DIM],t_dir=0,ts=0; // position, direction, state of the turmite
    
    unsigned char turmite[N_STATES*N_COLORS*3]; // turmite[i] is an index into possible_entries[i]
    vector<vector<unsigned char> > possible_entries(N_STATES*N_COLORS*3);
    
    int iDim,n_halts,iEntry,iCell,iColor,iState;
    bool satisfied,halted,off_grid;
    unsigned char color,new_color,new_dir,turn;

    unsigned long long tried=0,tested=0,until_print=PRINT_EVERY;
    int its,max_its=-1;
    int n_nonzero,max_nonzero=-1;
    
    // initialize the turmite and fix some entries
    memset(&turmite[0],0,sizeof(unsigned char)*N_STATES*N_COLORS*3);
    for(int iState=0;iState<N_STATES;iState++)
    {
        for(int iColor=0;iColor<N_COLORS;iColor++)
        {
            for(int i=0;i<N_COLORS;i++)
                possible_entries[encode(iState,iColor,0,N_COLORS)].push_back(i);
            for(int i=0;i<N_TURNS;i++)
                possible_entries[encode(iState,iColor,1,N_COLORS)].push_back(i);
            for(int i=0;i<N_STATES;i++)
                possible_entries[encode(iState,iColor,2,N_COLORS)].push_back(i);
        }
    }
    // first color printed can only be 0 or 1 by symmetry
    possible_entries[0].clear();
    possible_entries[0].push_back(0);
    possible_entries[0].push_back(1);
    // first turn can only be R or U (not L or halt) by symmetry
    possible_entries[1].clear();
    possible_entries[1].push_back(1);
    possible_entries[1].push_back(3);
    // first state can be 0 or 1 (not higher) by symmetry
    possible_entries[2].clear();
    possible_entries[2].push_back(0);
    possible_entries[2].push_back(1);

    ostringstream oss;
    oss << "found_tri_" << N_DIM << "d_";
    oss << N_STATES << "s_" << N_COLORS << "c.txt";
    ofstream out(oss.str().c_str());

    cout << "Saving results to: " << oss.str() << endl;
    // compute how far we've got to go
    unsigned long long target=1;
    for(int iEntry=0;iEntry<3*N_STATES*N_COLORS;iEntry++)
        target *= possible_entries[iEntry].size();
    out << "Total number of machines: " << target << endl;
    cout << "Total number of machines: " << target << endl;

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
        t_dir = 1; // starting orientation (arbitrary)
        halted=false;
        n_nonzero=0;
        for(its=0;its<ITS;its++)
        {
            iCell = t_pos[0];
            for(iDim=1;iDim<N_DIM;iDim++) iCell = iCell*SIDE + t_pos[iDim];
            color = grid[iCell];
			// what is the new direction of the turmite?
			turn = possible_entries[encode(ts,color,1,N_COLORS)][turmite[encode(ts,color,1,N_COLORS)]];
            new_dir = DIR_AFTER_TURN[t_dir][turn];
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
			// turmite moves:
			bool tri_pointing_up = ((t_pos[0]+t_pos[1])%2==0);
			if(tri_pointing_up)
			{
				t_pos[0] += UP_TURN[turn][t_dir][0];
				t_pos[1] += UP_TURN[turn][t_dir][1];
			}
			else
			{
				t_pos[0] += DOWN_TURN[turn][t_dir][0];
				t_pos[1] += DOWN_TURN[turn][t_dir][1];
			}
            off_grid = false;
            for(iDim=0;iDim<N_DIM;iDim++)
            {
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
                        out << TURN_TEXT[possible_entries[encode(iState,iColor,1,N_COLORS)][turmite[encode(iState,iColor,1,N_COLORS)]]] << ",";
                        out << (int)possible_entries[encode(iState,iColor,2,N_COLORS)][turmite[encode(iState,iColor,2,N_COLORS)]];
                        out << "}";
                    }
                    out << "}";
                }
                out << "}" << endl;
                if(true)
                {
                    // also save the image
					cvSet(image,cvScalar(0));
					CvPoint **pts = new CvPoint*[1];
					const int npts=3;
					pts[0] = new CvPoint[npts];
					uchar col;
					for(int y=0;y<SIDE;y++)
					{
						for(int x=0;x<SIDE;x++)
						{
							if(grid[x*SIDE+y]>0)
							{
								// draw triangle
								if((x+y)%2)
								{
									// down-pointing triangle
									pts[0][0] = cvPoint(TRI_BASE/2*(x-1),(y-1)*TRI_HEIGHT);
									pts[0][1] = cvPoint(TRI_BASE/2*(x+1),(y-1)*TRI_HEIGHT);
									pts[0][2] = cvPoint(TRI_BASE/2*x,y*TRI_HEIGHT);
								}
								else
								{
									// up-pointing triangle
									pts[0][0] = cvPoint(TRI_BASE/2*(x-1),y*TRI_HEIGHT);
									pts[0][1] = cvPoint(TRI_BASE/2*(x+1),y*TRI_HEIGHT);
									pts[0][2] = cvPoint(TRI_BASE/2*x,(y-1)*TRI_HEIGHT);
								}
								col = 255 * grid[x*SIDE+y] / (N_COLORS-1);
								cvFillConvexPoly(image,pts[0],3,cvScalar(col,col,col));
								cvPolyLine(image,pts,&npts,1,1,cvScalar(0,0,0),2,CV_AA);
							}
						}
					}
					delete []pts[0];
					delete []pts;
                    char fn[1000];
                    sprintf(fn,"tri_%d-%d_%dsteps_%dcells.png",N_STATES,N_COLORS,its,n_nonzero);
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