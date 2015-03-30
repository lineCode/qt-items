#include "CacheSpace.h"
#include "core/ControllerMouse.h"
#include "cache/CacheItem.h"
#include "cache/CacheItemFactory.h"
#include "utils/auto_value.h"

namespace Qi
{

CacheSpace::CacheSpace(const QSharedPointer<Space>& space, ViewApplicationMask viewApplicationMask)
    : m_space(space),
      m_viewApplicationMask(viewApplicationMask),
      m_window(0, 0, 0, 0),
      m_scrollOffset(0, 0),
      m_scrollDelta(0, 0),
      m_sizeDelta(0, 0),
      m_itemsCacheInvalid(true),
      m_cacheIsInUse(false)
{
    connect(m_space.data(), &Space::spaceChanged, this, &CacheSpace::onSpaceChanged);

    m_cacheItemsFactory = m_space->createCacheItemFactory(m_viewApplicationMask);
    Q_ASSERT(m_cacheItemsFactory);
}

CacheSpace::~CacheSpace()
{
    disconnect(m_space.data(), &Space::spaceChanged, this, &CacheSpace::onSpaceChanged);
}

void CacheSpace::onSpaceChanged(const Space* space, ChangeReason reason)
{
    Q_UNUSED(space);
    Q_ASSERT(space == m_space.data());

    if (reason & ChangeReasonSpaceStructure)
    {
        // invalidate all items
        clear();
    }
    else if (reason & (ChangeReasonSpaceHint | ChangeReasonSpaceItemsStructure))
    {
        // update items factory
        updateCacheItemsFactory();
        emit cacheChanged(this, reason|ChangeReasonCacheContent);
    }
    else if (reason & ChangeReasonSpaceItemsContent)
    {
        // forward event
        emit cacheChanged(this, reason|ChangeReasonCacheContent);
    }
}

void CacheSpace::setViewApplicationMask(ViewApplicationMask viewApplicationMask)
{
    if (m_viewApplicationMask != viewApplicationMask)
    {
        updateCacheItemsFactory();
        emit cacheChanged(this, ChangeReasonCacheContent);
    }
}

void CacheSpace::setWindow(const QRect& window)
{
    if (m_window == window)
        return;

    QRect _window = window.normalized();

    QPoint delta = _window.topLeft() - m_window.topLeft();
    m_scrollDelta += delta;
    m_sizeDelta += (_window.size() - m_window.size());
    m_window = _window;

    invalidateItemsCache();
}

void CacheSpace::setScrollOffset(const QPoint& scrollOffset)
{
    if (m_scrollOffset == scrollOffset)
        return;

    QPoint offset = m_scrollOffset - scrollOffset;
    m_scrollDelta += offset;
    m_scrollOffset = scrollOffset;

    invalidateItemsCache();
}

void CacheSpace::set(const QRect& window, const QPoint& scrollOffset)
{
    setWindow(window);
    setScrollOffset(scrollOffset);
}

QPoint CacheSpace::window2Space(const QPoint& windowPoint) const
{
    return windowPoint - m_window.topLeft() + m_scrollOffset;
}

QPoint CacheSpace::space2Window(const QPoint& spacePoint) const
{
    return spacePoint - m_scrollOffset + m_window.topLeft();
}

void CacheSpace::clear()
{
    clearItemsCache();
    invalidateItemsCache();
}

void CacheSpace::invalidateItemsCache()
{
    Q_ASSERT(!m_cacheIsInUse);
    m_itemsCacheInvalid = true;

    emit cacheChanged(this, ChangeReasonCacheContent);
}

void CacheSpace::clearItemsCache() const
{
    Q_ASSERT(!m_cacheIsInUse);
    clearItemsCacheImpl();
}

QSharedPointer<CacheItem> CacheSpace::createCacheItem(const ItemID& visibleItem) const
{
    return QSharedPointer<CacheItem>::create(m_cacheItemsFactory->create(visibleItem));
}

void CacheSpace::validateItemsCache() const
{
    if (!m_itemsCacheInvalid)
        return;

    validateItemsCacheImpl();
}

const CacheItem* CacheSpace::cacheItem(const ItemID& visibleItem) const
{
    validateItemsCache();
    return cacheItemImpl(visibleItem);
}

const CacheItem* CacheSpace::cacheItemByPosition(const QPoint& point) const
{
    validateItemsCache();
    return cacheItemByPositionImpl(point);
}

bool CacheSpace::forEachCacheItem(const std::function<bool(const QSharedPointer<CacheItem>&)>& visitor) const
{
    Q_ASSERT(visitor);
    return forEachCacheItemImpl(visitor);
}

bool CacheSpace::forEachCacheView(const std::function<bool(const CacheSpace::IterateInfo&)>& visitor) const
{
    Q_ASSERT(visitor);

    IterateInfo info;
    return forEachCacheItem([&visitor, &info](const QSharedPointer<CacheItem>& cacheItem)->bool {

        bool result = true;

        info.cacheItem = cacheItem;
        info.cacheViewIndex = 0;
        auto rootCacheView = cacheItem->cacheView();
        if (rootCacheView)
        {
            result = rootCacheView->forEachCacheView([&visitor, &info](CacheView* cacheView)->bool {
                info.cacheView = cacheView;
                bool result = visitor(info);
                ++info.cacheViewIndex;
                return result;
            });
        }
        ++info.cacheItemIndex;
        return result;
    });
}
/*
bool CacheSpace::forEachCacheView(const std::function<bool(const QSharedPointer<CacheItem>&, CacheView*)>& visitor)
{
    Q_ASSERT(visitor);

    return forEachCacheItem([&visitor](const QSharedPointer<CacheItem>& cacheItem)->bool {
        return item->forEachCacheView([&visitor, &cacheItem](const CacheView* cacheView)->bool {
                                          return visitor(cacheItem, cacheView);
                                      });
    });
}
*/

void CacheSpace::draw(QPainter* painter, const GuiContext& ctx) const
{
    validateItemsCache();

    auto_value<bool> inUse(m_cacheIsInUse, true);

    forEachCacheItem([&ctx, this](const QSharedPointer<CacheItem>& cacheItem)->bool {
                         cacheItem->validateCacheView(ctx, &m_window);
                         return true;
                     });

    emit preDraw();

    painter->save();
    painter->setClipRect(m_window);

    forEachCacheItem([painter, &ctx, this](const QSharedPointer<CacheItem>& cacheItem)->bool {
                         cacheItem->draw(painter, ctx, &m_window);
                         return true;
                     });

    painter->restore();
}

void CacheSpace::tryActivateControllers(const ControllerContext& context, QVector<ControllerMouse*>& controllers) const
{
    validateItemsCache();

    auto_value<bool> inUse(m_cacheIsInUse, true);

    const CacheItem* cacheItem = cacheItemByPosition(context.point);

    if (!cacheItem)
        return;

    cacheItem->tryActivateControllers(context, *this, &m_window, controllers);
}

bool CacheSpace::tooltipByPoint(const QPoint& point, TooltipInfo &tooltipInfo) const
{
    validateItemsCache();

    auto_value<bool> inUse(m_cacheIsInUse, true);

    const CacheItem* cacheItem = cacheItemByPosition(point);

    if (!cacheItem)
        return false;

    return cacheItem->tooltipByPoint(point, tooltipInfo);
}

void CacheSpace::updateCacheItemsFactory()
{
    m_cacheItemsFactory = m_space->createCacheItemFactory(m_viewApplicationMask);
    Q_ASSERT(m_cacheItemsFactory);

    // update schemas
    forEachCacheItem([this](const QSharedPointer<CacheItem>& cacheItem)->bool {
                         cacheItem->invalidateCacheView();
                         m_cacheItemsFactory->updateSchema(*cacheItem);
                         return true;
                     });
}


} // end namespace Qi
