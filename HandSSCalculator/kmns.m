function [ c, ic1, nc, wss, ifault ] = kmns ( a, m, n, c, k, iter )

%*****************************************************************************80
%
%% KMNS carries out the K-means algorithm.
%
%  Discussion:
%
%    This routine attempts to divide M points in N-dimensional space into
%    K clusters so that the within cluster sum of squares is minimized.
%
%  Licensing:
%
%    This code is distributed under the GNU LGPL license.
%
%  Modified:
%
%    15 February 2008
%
%  Author:
%
%    Oiginal FORTRAN77 version by John Hartigan, Manchek Wong.
%    MATLAB version by John Burkardt.
%
%  Reference:
%
%    John Hartigan, Manchek Wong,
%    Algorithm AS 136:
%    A K-Means Clustering Algorithm,
%    Applied Statistics,
%    Volume 28, Number 1, 1979, pages 100-108.
%
%  Parameters:
%
%    Input, real A(M,N), the points.
%
%    Input, integer M, the number of points.
%
%    Input, integer N, the number of spatial dimensions.
%
%    Input, real C(K,N), the initial cluster centers.
%
%    Input, integer K, the number of clusters.
%
%    Input, integer ITER, the maximum number of iterations allowed.
%
%    Output, real C(K,N), the final cluster centers.
%
%    Output, integer IC1(M), the cluster to which each point
%    is assigned.
%
%    Output, integer NC(K), the number of points in each cluster.
%
%    Output, real WSS(K), the within-cluster sum of squares
%    of each cluster.
%
%    Output, integer IFAULT, error indicator.
%    0, no error was detected.
%    1, at least one cluster is empty after the initial assignment.  A better
%       set of initial cluster centers is needed.
%    2, the allowed maximum number off iterations was exceeded.
%    3, K is less than or equal to 1, or greater than or equal to M.
%
  ifault = 0;
  ic1 = [];
  nc = [];
  wss = [];


  if ( k <= 1 | m <= k )
    ifault = 3;
    return
  end
%
%  For each point I, find its two closest centers, IC1(I) and
%  IC2(I).  Assign the point to IC1(I).
%
  for i = 1 : m

    ic1(i) = 1;
    ic2(i) = 2;

    for il = 1 : 2
      dt(il) = 0.0;
      for j = 1 : n
        da = a(i,j) - c(il,j);
        dt(il) = dt(il) + da * da;
      end
    end

    if ( dt(2) < dt(1) )
      ic1(i) = 2;
      ic2(i) = 1;
      temp = dt(1);
      dt(1) = dt(2);
      dt(2) = temp;
    end

    for l = 3 : k

      db = 0.0;
      for j = 1 : n
        dc = a(i,j) - c(l,j);
        db = db + dc * dc;
      end

      if ( db < dt(2) )

        if ( dt(1) <= db )
          dt(2) = db;
          ic2(i) = l;
        else
          dt(2) = dt(1);
          ic2(i) = ic1(i);
          dt(1) = db;
          ic1(i) = l;
        end

      end

    end

  end
%
%  Update cluster centers to be the average of points contained within them.
%
  for l = 1 : k
    nc(l) = 0;
    for j = 1 : n
      c(l,j) = 0.0;
    end
  end

  for i = 1 : m
    l = ic1(i);
    nc(l) = nc(l) + 1;
    for j = 1 : n
      c(l,j) = c(l,j) + a(i,j);
    end
  end
%
%  Check to see if there is any empty cluster at this stage.
%
  ifault = 1;

  for l = 1 : k

    if ( nc(l) == 0 )
      ifault = 1;
      return
    end

  end

  ifault = 0;

  for l = 1 : k

    aa = nc(l);

    for j = 1 : n
      c(l,j) = c(l,j) / aa;
    end
%
%  Initialize AN1, AN2, ITRAN and NCP.
%
%  AN1(L) = NC(L) / (NC(L) - 1)
%  AN2(L) = NC(L) / (NC(L) + 1)
%  ITRAN(L) = 1 if cluster L is updated in the quick-transfer stage,
%           = 0 otherwise
%
%  In the optimal-transfer stage, NCP(L) stores the step at which
%  cluster L is last updated.
%
%  In the quick-transfer stage, NCP(L) stores the step at which
%  cluster L is last updated plus M.
%
    an2(l) = aa / ( aa + 1.0 );

    if ( 1.0 < aa )
      an1(l) = aa / ( aa - 1.0 );
    else
      an1(l) = Inf;
    end

    itran(l) = 1;
    ncp(l) = -1;

  end

  indx = 0;
  d(1:m) = 0.0;
  live(1:k) = 0;
  ifault = 2;

  for ij = 1 : iter
%
%  In this stage, there is only one pass through the data.   Each
%  point is re-allocated, if necessary, to the cluster that will
%  induce the maximum reduction in within-cluster sum of squares.
%
  [ c, ic1, ic2, nc, an1, an2, ncp, d, itran, live, indx ] = ...
    optra ( a, m, n, c, k, ic1, ic2, nc, an1, an2, ncp, d, itran, live, indx );
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%% begin optra %%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

function [ c, ic1, ic2, nc, an1, an2, ncp, d, itran, live, indx ] = ...
  optra ( a, m, n, c, k, ic1, ic2, nc, an1, an2, ncp, d, itran, live, indx )

%*****************************************************************************80
%
%% OPTRA carries out the optimal transfer stage.
%
%  Discussion:
%
%    This is the optimal transfer stage.
%
%    Each point is re-allocated, if necessary, to the cluster that
%    will induce a maximum reduction in the within-cluster sum of
%    squares.
%
%  Licensing:
%
%    This code is distributed under the GNU LGPL license.
%
%  Modified:
%
%    15 February 2008
%
%  Author:
%
%    Original FORTRAN77 version by John Hartigan, Manchek Wong.
%    MATLAB version by John Burkardt.
%
%  Reference:
%
%    John Hartigan, Manchek Wong,
%    Algorithm AS 136:
%    A K-Means Clustering Algorithm,
%    Applied Statistics,
%    Volume 28, Number 1, 1979, pages 100-108.
%
%  Parameters:
%
%    Input, real A(M,N), the points.
%
%    Input, integer M, the number of points.
%
%    Input, integer N, the number of spatial dimensions.
%
%    Input/output, real C(K,N), the cluster centers.
%
%    Input, integer K, the number of clusters.
%
%    Input/output, integer IC1(M), the cluster to which each
%    point is assigned.
%
%    Input/output, integer IC2(M), used to store the cluster
%    which each point is most likely to be transferred to at each step.
%
%    Input/output, integer NC(K), the number of points in
%    each cluster.
%
%    Input/output, real AN1(K).
%
%    Input/output, real AN2(K).
%
%    Input/output, integer NCP(K).
%
%    Input/output, real D(M).
%
%    Input/output, integer ITRAN(K).
%
%    Input/output, integer LIVE(K).
%
%    Input/output, integer INDX, the number of steps since a
%    transfer took place.
%

%
%  If cluster L is updated in the last quick-transfer stage, it
%  belongs to the live set throughout this stage.   Otherwise, at
%  each step, it is not in the live set if it has not been updated
%  in the last M optimal transfer steps.
%
  for l = 1 : k
    if ( itran(l) == 1)
      live(l) = m + 1;
    end
  end

  for i = 1 : m

    indx = indx + 1;
    l1 = ic1(i);
    l2 = ic2(i);
    ll = l2;
%
%  If point I is the only member of cluster L1, no transfer.
%
    if ( 1 < nc(l1)  )
%
%  If L1 has not yet been updated in this stage, no need to
%  re-compute D(I).
%
      if ( ncp(l1) ~= 0 )
        de = 0.0;
        for j = 1 : n
          df = a(i,j) - c(l1,j);
          de = de + df * df;
        end
        d(i) = de * an1(l1);
      end
%
%  Find the cluster with minimum R2.
%
     da = 0.0;
      for j = 1 : n
        db = a(i,j) - c(l2,j);
        da = da + db * db;
      end
      r2 = da * an2(l2);

      for l = 1 : k
%
%  If LIVE(L1) <= I, then L1 is not in the live set.   If this is
%  true, we only need to consider clusters that are in the live set
%  for possible transfer of point I.   Otherwise, we need to consider
%  all possible clusters.
%
        if ( ( i < live(l1) | i < live(l2) ) & l ~= l1 & l ~= ll )

          rr = r2 / an2(l);

          dc = 0.0;
          for j = 1 : n
            dd = a(i,j) - c(l,j);
            dc = dc + dd * dd;
          end

          if ( dc < rr )
            r2 = dc * an2(l);
            l2 = l;
          end

        end

      end
%
%  If no transfer is necessary, L2 is the new IC2(I).
%
      if ( d(i) <= r2 )

        ic2(i) = l2;
%
%  Update cluster centers, LIVE, NCP, AN1 and AN2 for clusters L1 and
%  L2, and update IC1(I) and IC2(I).
%
      else

        indx = 0;
        live(l1) = m + i;
        live(l2) = m + i;
        ncp(l1) = i;
        ncp(l2) = i;
        al1 = nc(l1);
        alw = al1 - 1.0;
        al2 = nc(l2);
        alt = al2 + 1.0;
        for j = 1 : n
          c(l1,j) = ( c(l1,j) * al1 - a(i,j) ) / alw;
          c(l2,j) = ( c(l2,j) * al2 + a(i,j) ) / alt;
        end
        nc(l1) = nc(l1) - 1;
        nc(l2) = nc(l2) + 1;
        an2(l1) = alw / al1;
        if ( 1.0 < alw )
          an1(l1) = alw / ( alw - 1.0 );
        else
          an1(l1) = Inf;
        end
        an1(l2) = alt / al2;
        an2(l2) = alt / ( alt + 1.0 );
        ic1(i) = l2;
        ic2(i) = l1;

      end

    end

    if ( indx == m )
      return
    end

  end
%
%  ITRAN(L) = 0 before entering QTRAN.   Also, LIVE(L) has to be
%  decreased by M before re-entering OPTRA.
%
  for l = 1 : k
    itran(l) = 0;
    live(l) = live(l) - m;
  end

  return
end
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%% end optra %%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%
%  Stop if no transfer took place in the last M optimal transfer steps.
%
    if ( indx == m )
      ifault = 0;
      break
    end
%
%  Each point is tested in turn to see if it should be re-allocated
%  to the cluster to which it is most likely to be transferred,
%  IC2(I), from its present cluster, IC1(I).   Loop through the
%  data until no further change is to take place.
%
  [ c, ic1,ic2, nc, an1, an2, ncp, d, itran, indx ] = qtran ( a, m, ...
    n, c, k, ic1, ic2, nc, an1, an2, ncp, d, itran, indx );
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%% begin qtran %%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

function [ c, ic1,ic2, nc, an1, an2, ncp, d, itran, indx ] = qtran ( a, m, ...
  n, c, k, ic1, ic2, nc, an1, an2, ncp, d, itran, indx )

%*****************************************************************************80
%
%% QTRAN carries out the quick transfer stage.
%
%  Discussion:
%
%    This is the quick transfer stage.
%
%    IC1(I) is the cluster which point I belongs to.
%    IC2(I) is the cluster which point I is most likely to be
%    transferred to.
%
%    For each point I, IC1(I) and IC2(I) are switched, if necessary, to
%    reduce within-cluster sum of squares.  The cluster centers are
%    updated after each step.
%
%  Licensing:
%
%    This code is distributed under the GNU LGPL license.
%
%  Modified:
%
%    15 February 2008
%
%  Author:
%
%    Original FORTRAN77 version by John Hartigan, Manchek Wong.
%    MATLAB version by John Burkardt.
%
%  Reference:
%
%    John Hartigan, Manchek Wong,
%    Algorithm AS 136:
%    A K-Means Clustering Algorithm,
%    Applied Statistics,
%    Volume 28, Number 1, 1979, pages 100-108.
%
%  Parameters:
%
%    Input, real A(M,N), the points.
%
%    Input, integer M, the number of points.
%
%    Input, integer N, the number of spatial dimensions.
%
%    Input/output, real C(K,N), the cluster centers.
%
%    Input, integer K, the number of clusters.
%
%    Input/output, integer IC1(M), the cluster to which each
%    point is assigned.
%
%    Input/output, integer IC2(M), used to store the cluster
%    which each point is most likely to be transferred to at each step.
%
%    Input/output, integer NC(K), the number of points in
%    each cluster.
%
%    Input/output, real AN1(K).
%
%    Input/output, real AN2(K).
%
%    Input/output, integer NCP(K).
%
%    Input/output, real D(M).
%
%    Input/output, integer ITRAN(K).
%
%    Input/output, integer INDX, counts the number of steps
%    since the last transfer.
%

%
%  In the optimal transfer stage, NCP(L) indicates the step at which
%  cluster L is last updated.   In the quick transfer stage, NCP(L)
%  is equal to the step at which cluster L is last updated plus M.
%
  icoun = 0;
  istep = 0;

  while ( 1 )

    for i = 1 : m

      icoun = icoun + 1;
      istep = istep + 1;
      l1 = ic1(i);
      l2 = ic2(i);
%
%  If point I is the only member of cluster L1, no transfer.
%
      if ( 1 < nc(l1) )
%
%  If NCP(L1) < ISTEP, no need to re-compute distance from point I to
%  cluster L1.   Note that if cluster L1 is last updated exactly M
%  steps ago, we still need to compute the distance from point I to
%  cluster L1.
%
        if ( istep <= ncp(l1) )

          da = 0.0;
          for j = 1 : n
            db = a(i,j) - c(l1,j);
            da = da + db * db;
          end

          d(i) = da * an1(l1);

        end
%
%  If NCP(L1) <= ISTEP and NCP(L2) <= ISTEP, there will be no transfer of
%  point I at this step.
%
        if ( istep < ncp(l1) | istep < ncp(l2) )

          r2 = d(i) / an2(l2);

          dd = 0.0;
          for j = 1 : n
            de = a(i,j) - c(l2,j);
            dd = dd + de * de;
          end
%
%  Update cluster centers, NCP, NC, ITRAN, AN1 and AN2 for clusters
%  L1 and L2.   Also update IC1(I) and IC2(I).   Note that if any
%  updating occurs in this stage, INDX is set back to 0.
%
          if ( dd < r2 )

            icoun = 0;
            indx = 0;
            itran(l1) = 1;
            itran(l2) = 1;
            ncp(l1) = istep + m;
            ncp(l2) = istep + m;
            al1 = nc(l1);
            alw = al1 - 1.0;
            al2 = nc(l2);
            alt = al2 + 1.0;
            for j = 1 : n
              c(l1,j) = ( c(l1,j) * al1 - a(i,j) ) / alw;
              c(l2,j) = ( c(l2,j) * al2 + a(i,j) ) / alt;
            end
            nc(l1) = nc(l1) - 1;
            nc(l2) = nc(l2) + 1;
            an2(l1) = alw / al1;
            if ( 1.0 < alw )
              an1(l1) = alw / ( alw - 1.0 );
            else
              an1(l1) = Inf;
            end
            an1(l2) = alt / al2;
            an2(l2) = alt / ( alt + 1.0 );
            ic1(i) = l2;
            ic2(i) = l1;

          end

        end

      end
%
%  If no re-allocation took place in the last M steps, return.
%
      if ( icoun == m )
        return
      end

    end

  end

  return
end
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%% end qtran %%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  If there are only two clusters, there is no need to re-enter the
%  optimal transfer stage.
%
    if ( k == 2 )
      ifault = 0;
      break
    end
%
%  NCP has to be set to 0 before entering OPTRA.
%
    for l = 1 : k
      ncp(l) = 0;
    end

  end
%
%  If the maximum number of iterations was taken without convergence,
%  IFAULT is 2 now.  This may indicate unforeseen looping.
%
  if ( ifault == 2 )
    fprintf ( 1, '\n' );
    fprintf ( 1, 'KMNS - Warning!\n' );
    fprintf ( 1, '  Maximum number of iterations reached\n' );
    fprintf ( 1, '  without convergence.\n' );
  end
%
%  Compute the within-cluster sum of squares for each cluster.
%
  for l = 1 : k
    wss(l) = 0.0;
    for j = 1 : n
      c(l,j) = 0.0;
    end
  end

  for i = 1 : m
    ii = ic1(i);
    for j = 1 : n
      c(ii,j) = c(ii,j) + a(i,j);
    end
  end

  for j = 1 : n
    for l = 1 : k
      c(l,j) = c(l,j) / nc(l);
    end
    for i = 1 : m
      ii = ic1(i);
      da = a(i,j) - c(ii,j);
      wss(ii) = wss(ii) + da * da;
    end
  end

  return
end
