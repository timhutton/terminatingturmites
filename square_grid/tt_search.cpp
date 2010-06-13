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

int encode(int state,int color,int element,int N_COLORS)
{
	return state*N_COLORS*3 + color*3 + element;
}

int main()
{
    // ---------------- things a casual user will want to experiment with -----------------------

    // choose the type of turmite you want to search for:
    const int N_DIM=3; // 1D, 2D, 3D, etc.
    const int N_STATES=4;
    const int N_COLORS=2;
    const bool relative_movement = false; // true: relative Turmites ("TurNing machines"), false: absolute Turmites (Turing machines)

    // specify some constraints we need to help us search
    const int ITS=10000; // Limitation of this approach: if BB lasts longer than this we'll miss it
    const int R=20; // square radius. Limitation: if BB spreads more than this in any direction we'll miss it

    const unsigned long long PRINT_EVERY=10000; // how often to report back

    // ------------------------------------------------------------------------------------------

    if(relative_movement && N_DIM>=3)
    {
        cout << "We only support relative turmites for 1D and 2D." << endl;
        // The difficulty lies in the turmite orientation. In 1D and 2D we only need store which
        // direction the turmite last moved in, this is sufficient to determine its orientation. In
        // 3D, however, we also need a twist orientation: an airplane flying north that turns right will 
        // be travelling east if it's flying the right way up, west if it's flying upside-down. In 
        // general we need D-1 values for dimensions D>1 (I think?) and my brain hurts when I try to 
        // visualise this.
        exit(1);
    }

    const int SIDE=2*R+1;

    // initialise DIRS for each direction: 0=halt, then 2 for each axis X,Y,Z etc.
    const int N_DIRS=1+N_DIM*2;
    int DIRS[N_DIRS][N_DIM];
    for(int iDim=0;iDim<N_DIM;iDim++)
    {
        DIRS[0][iDim]=0; // first move is a halt state, so it actually doesn't matter what's here
        for(int iDim2=0;iDim2<N_DIM;iDim2++)
        {
            if(iDim2==iDim)
            {
                DIRS[1+iDim*2+0][iDim2] = 1; // positive direction along this axis
                DIRS[1+iDim*2+1][iDim2] = -1; // negative direction along this axis
            }
            else
            {
                DIRS[1+iDim*2+0][iDim2] = 0;
                DIRS[1+iDim*2+1][iDim2] = 0;
            }
        }
    }
    // for output, what is the label for each direction
    const string DIR_TEXT_KNOWN_ABSOLUTE[7] = {"''","'E'","'W'","'N'","'S'","'U'","'D'"};
    // for output, what is the label for each turn (Ed Pegg Jr.'s Turmite notation)
    const string DIR_TEXT_KNOWN_RELATIVE[5] = {"0","1","4","2","8"}; // 0=halt, 1=noturn, 4=u-turn, 2=right, 8=left (Ed Pegg's notation, we output for Turmite-gen.py)
    const int DIR_AFTER_TURN[5][5] = // new_dir = DIR_AFTER_TURN[old_dir][turn]
        {{0,0,0,0,0},{0,1,2,4,3},{0,2,1,3,4},{0,3,4,1,2},{0,4,3,2,1}};
    string DIR_TEXT[N_DIRS];
    for(int i=0;i<min(7,N_DIRS);i++) 
        DIR_TEXT[i] = relative_movement?DIR_TEXT_KNOWN_RELATIVE[i]:DIR_TEXT_KNOWN_ABSOLUTE[i];
    for(int iDim=3;iDim<N_DIM;iDim++)
    {
        // for 4D etc. we just label the 'compass directions' as "4+", "4-", "5+", "5-", etc.
        { ostringstream oss; oss << iDim+1 << "+"; DIR_TEXT[1+iDim*2+0] = oss.str(); }
        { ostringstream oss; oss << iDim+1 << "-"; DIR_TEXT[1+iDim*2+1] = oss.str(); }
    }

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
    oss << "found_" << N_DIM << "d_";
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
                for(int i=0;i<N_DIRS;i++)
                    possible_entries[encode(iState,iColor,1,N_COLORS)].push_back(i);
            }
            else
            {
                // no halt state for states >2 (by symmetry)
                for(int i=1;i<N_DIRS;i++)
                    possible_entries[encode(iState,iColor,1,N_COLORS)].push_back(i);
            }
            for(int i=0;i<N_STATES;i++)
                possible_entries[encode(iState,iColor,2,N_COLORS)].push_back(i);
        }
    }
    if(relative_movement)
    {
        // first color printed can only be 0 or 1 by symmetry
        possible_entries[0].clear();
        possible_entries[0].push_back(0);
        possible_entries[0].push_back(1);
        // first move can only be F,B, or R (not L or H) by symmetry
        possible_entries[1].clear();
        possible_entries[1].push_back(1);
        possible_entries[1].push_back(2);
        possible_entries[1].push_back(3);
        // first state can be 0 or 1 (not higher) by symmetry
        possible_entries[2].clear();
        possible_entries[2].push_back(0);
        possible_entries[2].push_back(1);
    }
    else
    {
        // first triple is fixed at {1,'E',1} because of symmetry
        // -if first print is 0 then can just take the destination state as the starting one)
        // -if first print is >1 then by rotating the colors around can make it 1
        // -if first transition is to state 0 then turmite will zip off
        // -if first transition is to state >1 then by rotating the states can make it 1
        // -if first move is anything other than North, can just rotate it round
        possible_entries[0].clear();
        possible_entries[0].push_back(1);
        possible_entries[1].clear();
        possible_entries[1].push_back(1);
        possible_entries[2].clear();
        possible_entries[2].push_back(1);
    }

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
            if(satisfied && !relative_movement)
            {
                // does turmite return to state 0 after first transition and doesn't move 'W'?
				// (we know first transition is to state 1)
                if(possible_entries[encode(1,0,2,N_COLORS)][turmite[encode(1,0,2,N_COLORS)]]==0 
					&& possible_entries[encode(1,0,1,N_COLORS)][turmite[encode(1,0,1,N_COLORS)]]!=2)
                {		
					// turmite zips off
                    satisfied=false;
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
                new_dir = DIR_AFTER_TURN[t_dir][possible_entries[encode(ts,color,1,N_COLORS)][turmite[encode(ts,color,1,N_COLORS)]]];
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
