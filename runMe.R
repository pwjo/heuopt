COMMAND <- "src/assignment4"
SEL_INSTANCE <- "instances/automatic-4.txt"

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
runMe <- function(nAnts,nRuns,instance,alpha,beta,script,n,x=NULL) {
    res <- numeric(n)
    timings <- numeric(n)
    for(i in 1:n) {
        outFileString <- paste("output/aco_out_",gsub("/","_",instance),x,n,".txt",sep="")
        commandString <- paste(script,"--num-ants",nAnts,"--num-runs",nRuns,"--beta",beta,"--alpha",alpha,
                               "--input",instance,"--output",outFileString)
        timing <- system.time(out <- system(commandString,intern=TRUE))
        crossingSum <- tail(out,1)
        ## regexp would be nice, but so far, stringsplit at " " and take the third element of the resulting vector
        res[i] <- as.numeric(strsplit(crossingSum," ")[[1]][3])
        timings[i] <- timing["user.self"]
    }
    if(n==1) {
        return(list(res=res,avg=mean(res),timings=timings,avgTime=mean(timings)))
    } else {
        return(list(res=res,avg=mean(res),stdDevs=sd(res),timings=timings,avgTime=mean(timings)))
    }
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


library(parallel)


alpha <- c(0.1,1,2)
beta <- c(0.1,1,2)
nRuns <- c(5,10,20,50,100)
nAnts <- c(100,500,1000,2000)
n <- 10
params <- expand.grid(alpha,beta,nRuns,nAnts,n)
colnames(params) <- c("alpha","beta","nRuns","nAnts","n")


optim_res <- mclapply(1:nrow(params),pRunMe,params=params,instance=SEL_INSTANCE,mc.cores=8)

## create object which will be saved
opt_res <- list(optim_res=optim_res,params=params)

optResFileString <- paste("output/aco_opt_",gsub("/","_",SEL_INSTANCE),".Rdata",sep="")
save(opt_res,file=optResFileString)


runMeWrapper <- function(x,nAnts,nRuns,instance,alpha,beta,script) {
    res1 <- runMe(nAnts=nAnts,nRuns=nRuns,instance=instance,alpha=alpha,beta=beta,script=script,n=1,x=x)
    return(res1)
}



mclapply(1:10,runMeWrapper,nAnts=50,nRuns=10,instance="instances/automatic-6.txt",alpha=2,beta=2,script=COMMAND)