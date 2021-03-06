##' execute C program and collect number of crossings
##'
##' This script executes one run of our C-program. It uses system() and also creates
##' the command string 
##' @title 
##' @param nAnts number of ants
##' @param nRuns number of runs
##' @param instance instance file
##' @param outFile output file
##' @param alpha 
##' @param beta 
##' @param script 
##' @return 
##' @author Alexander
runMe <- function(nAnts,nRuns,instance,outFile,alpha,beta,script,n,outerParallel=TRUE) {
    res <- numeric(n)
    timings <- numeric(n)
    for(i in 1:n) {
        outFileString <- paste("output/aco_out_",gsub("/","_",instance),n,".txt",sep="")
        commandString <- paste(script,"--num-ants",nAnts,"--num-runs",nRuns,"--beta",beta,"--alpha",alpha,
                               "--input",instance,"--output",outFileString)
        timing <- system.time(out <- system(commandString,intern=TRUE))
        crossingSum <- tail(out,1)
        ## regexp would be nice, but so far, stringsplit at " " and take the third element of the resulting vector
        res[i] <- as.numeric(strsplit(crossingSum," ")[[1]][3])
        timings[i] <- timing["user.self"]
    }
    
    return(list(res=res,avg=mean(res),stdDevs=sd(res),timings=timings,avgTime=mean(timings)))
}

##' Parallel version of runMe
##'
##' @title Parallel version of runMe
##' @param x identifier of this instance
##' @param params parameter matrix
##' @param instance which graph instance we should use
##' @param script C++ program to run
##' @param n number of runs
##' @return 
##' @author Alexander
pRunMe <- function(x,params,instance,script=COMMAND,n=20) {
    alpha <- params[x,"alpha"]
    beta <- params[x,"beta"]
    nRuns <- params[x,"nRuns"]
    nAnts <- params[x,"nAnts"]
    n <- params[x,"n"]
    res <- runMe(nAnts=nAnts,nRuns=nRuns,instance=instance,alpha=alpha,beta=beta,script=script,n=n)
    return(res)
}
