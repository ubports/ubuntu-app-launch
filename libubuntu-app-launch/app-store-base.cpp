
#include "app-store-base.h"

namespace ubuntu
{
namespace app_launch
{
namespace app_store
{

Base::Base()
    : info_watcher::Base({})
{
}

Base::~Base()
{
}

std::list<std::shared_ptr<Base>> Base::allAppStores()
{
    return {};
}

}  // namespace app_store
}  // namespace app_launch
}  // namespace ubuntu
