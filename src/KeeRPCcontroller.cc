#include "KeeRPCcontroller.h"

KeeRPCcontroller::KeeRPCcontroller(){
    m_failed = false;
    m_errText = "";
}

void KeeRPCcontroller::Reset(){
     m_failed = false;
    m_errText = "";
}

bool KeeRPCcontroller::Failed()const{
    return m_failed;
}

std::string KeeRPCcontroller::ErrorText()const{
    return m_errText;
}

void KeeRPCcontroller::SetFailed(const std::string &reason){
    m_failed = true;
    m_errText = reason;
}

void KeeRPCcontroller::StartCancel(){}

// 基类要求实现。没有取消功能，所以永远是未取消。
bool KeeRPCcontroller::IsCanceled() const {
    return false;
}

// 基类要求实现。callback 不保存，取消永远不会发生。
void KeeRPCcontroller::NotifyOnCancel(google::protobuf::Closure *callback) {
    (void)callback;
}